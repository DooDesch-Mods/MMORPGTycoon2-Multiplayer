#include "mt2mp/net/steam_avatars.h"
#include "mt2mp/net/avatar_png.h"
#include "mt2mp/net/steam_module.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/private_profile_paths.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace mt2mp::net::avatars {
namespace {
constexpr size_t max_members = 64, max_images = 128;
constexpr unsigned max_pending_polls = 30;
// Fit the longest axis and retain aspect ratio to whole pixels; existing small images stay byte-identical.
void fit_rgba(uint32_t& width, uint32_t& height, std::vector<unsigned char>& rgba) {
    const auto largest = (std::max)(width, height);
    if (largest <= 128) return;
    const uint32_t fitted_width = (std::max)(1u, width*128/largest);
    const uint32_t fitted_height = (std::max)(1u, height*128/largest);
    std::vector<unsigned char> fitted(size_t(fitted_width)*fitted_height*4);
    for (uint32_t y = 0; y < fitted_height; ++y) for (uint32_t x = 0; x < fitted_width; ++x) {
        const auto source = (size_t(y*height/fitted_height)*width + x*width/fitted_width)*4;
        const auto destination = (size_t(y)*fitted_width+x)*4;
        for (unsigned channel = 0; channel < 4; ++channel) fitted[destination+channel] = rgba[source+channel];
    }
    width = fitted_width; height = fitted_height; rgba = std::move(fitted);
}
Result failure(std::string cause) { return {State::Error, {}, std::move(cause), 0, 0}; }
std::string win_error(const char* operation, DWORD code = GetLastError()) {
    return std::string(operation) + " Win32=" + std::to_string(code);
}
bool cache_name(const std::wstring& name) {
    if (name.size() != 68 || name.substr(64) != L".png") return false;
    return std::all_of(name.begin(), name.begin()+64, [](wchar_t ch) { return (ch >= L'0' && ch <= L'9') || (ch >= L'a' && ch <= L'f'); });
}
struct FileHandle {
    HANDLE value;
    explicit FileHandle(HANDLE file) : value(file) {}
    ~FileHandle() {
        if (value != INVALID_HANDLE_VALUE && !CloseHandle(value))
            std::fprintf(stderr, "error: Avatar exception cleanup CloseHandle Win32=%lu\n", GetLastError());
    }
};
bool close_file(FileHandle& file, std::string& error) {
    const auto value = file.value; file.value = INVALID_HANDLE_VALUE;
    if (CloseHandle(value)) return true;
    error = win_error("Avatar cache CloseHandle"); return false;
}
bool verify_file(const std::filesystem::path& path, const std::vector<unsigned char>& expected, std::string& error) {
    FileHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (file.value == INVALID_HANDLE_VALUE) { error = win_error("Avatar cache read open"); return false; }
    FILE_ATTRIBUTE_TAG_INFO attributes{}; LARGE_INTEGER size{};
    bool ok = GetFileInformationByHandleEx(file.value, FileAttributeTagInfo, &attributes, sizeof(attributes)) != 0;
    if (!ok) error = win_error("Avatar cache read attributes");
    else if (attributes.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) {
        error = "Avatar cache file is a reparse point or directory"; ok = false;
    }
    if (ok && !GetFileSizeEx(file.value, &size)) { error = win_error("Avatar cache read size"); ok = false; }
    if (ok && size.QuadPart != static_cast<LONGLONG>(expected.size())) { error = "Avatar cache existing byte length differs"; ok = false; }
    std::vector<unsigned char> actual(expected.size()); DWORD read = 0;
    if (ok && !ReadFile(file.value, actual.data(), static_cast<DWORD>(actual.size()), &read, nullptr)) { error = win_error("Avatar cache ReadFile"); ok = false; }
    if (ok && (read != actual.size() || actual != expected)) { error = "Avatar cache existing content differs"; ok = false; }
    std::string cleanup;
    if (!close_file(file, cleanup)) { if (!error.empty()) error += "; "; error += cleanup; ok = false; }
    return ok;
}
bool publish_file(const std::filesystem::path& directory, const std::wstring& name,
                  const std::vector<unsigned char>& bytes, std::string& error) {
    const auto target = directory / name;
    const auto attributes = GetFileAttributesW(target.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) return verify_file(target, bytes, error);
    const auto missing = GetLastError();
    if (missing != ERROR_FILE_NOT_FOUND) { error = win_error("Avatar cache destination attributes", missing); return false; }
    std::array<unsigned char,16> nonce{};
    const auto random = BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (random < 0) { error = "Avatar cache nonce BCrypt NTSTATUS=" + std::to_string(random); return false; }
    const auto hex = private_profile::hex(nonce.data(), nonce.size());
    const auto temporary = directory / (L".avatar-" + std::wstring(hex.begin(), hex.end()) + L".tmp");
    FileHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.value == INVALID_HANDLE_VALUE) { error = win_error("Avatar cache temporary create"); return false; }
    DWORD written = 0;
    bool ok = WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) != 0;
    if (!ok) error = win_error("Avatar cache WriteFile");
    else if (written != bytes.size()) { error = "Avatar cache short write"; ok = false; }
    if (ok && !FlushFileBuffers(file.value)) { error = win_error("Avatar cache FlushFileBuffers"); ok = false; }
    std::string cleanup;
    if (!close_file(file, cleanup)) { if (!error.empty()) error += "; "; error += cleanup; ok = false; }
    if (ok && MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) return true;
    if (ok) {
        const auto move = GetLastError();
        if (move == ERROR_ALREADY_EXISTS || move == ERROR_FILE_EXISTS) ok = verify_file(target, bytes, error);
        else { error = win_error("Avatar cache publish rename", move); ok = false; }
    }
    if (!DeleteFileW(temporary.c_str())) { if (!error.empty()) error += "; "; error += win_error("Avatar cache temporary cleanup"); ok = false; }
    return ok;
}
}

struct Provider::Impl {
    struct Member { Result result; std::chrono::steady_clock::time_point next{}; unsigned polls = 0; };
    Api api;
    std::filesystem::path data, cache;
    std::unique_ptr<private_profile::DirectoryPins> pins;
    DWORD thread = 0;
    std::map<uint64_t, Member> members;
    std::map<int, Result> images;
    std::set<std::string> files;
    explicit Impl(Api input) : api(input) {}
    bool on_thread(std::string& error) const {
        if (!thread || thread != GetCurrentThreadId()) { error = "Avatar provider requires its configured game thread"; return false; }
        return true;
    }
    bool bind(std::string& error) {
        if (api.ready()) return true;
        const auto steam = steam_module();
        if (!steam) { error = "Avatar selected Steam module unavailable"; return false; }
        const auto load = [&](const char* name) { return GetProcAddress(steam, name); };
        const auto friends = reinterpret_cast<void*(*)()>(load("SteamAPI_SteamFriends_v018"));
        const auto utils = reinterpret_cast<void*(*)()>(load("SteamAPI_SteamUtils_v010"));
        api.large_avatar = reinterpret_cast<decltype(api.large_avatar)>(load("SteamAPI_ISteamFriends_GetLargeFriendAvatar"));
        api.image_size = reinterpret_cast<decltype(api.image_size)>(load("SteamAPI_ISteamUtils_GetImageSize"));
        api.image_rgba = reinterpret_cast<decltype(api.image_rgba)>(load("SteamAPI_ISteamUtils_GetImageRGBA"));
        if (!friends) { error = "Avatar missing Steam export SteamAPI_SteamFriends_v018"; return false; }
        if (!utils) { error = "Avatar missing Steam export SteamAPI_SteamUtils_v010"; return false; }
        if (!api.large_avatar) { error = "Avatar missing Steam export SteamAPI_ISteamFriends_GetLargeFriendAvatar"; return false; }
        if (!api.image_size) { error = "Avatar missing Steam export SteamAPI_ISteamUtils_GetImageSize"; return false; }
        if (!api.image_rgba) { error = "Avatar missing Steam export SteamAPI_ISteamUtils_GetImageRGBA"; return false; }
        api.friends = friends();
        if (!api.friends) { error = "Avatar existing SteamFriends interface not ready"; return false; }
        api.utils = utils();
        if (!api.utils) { error = "Avatar existing SteamUtils interface not ready"; return false; }
        return true;
    }
};

Provider::Provider(Api api) : impl_(std::make_unique<Impl>(api)) {}
Provider::~Provider() = default;
bool Provider::configure(const std::filesystem::path& directory, std::string& error) {
    try {
        error.clear();
        auto& state = *impl_;
        if (state.thread) { error = "Avatar provider is already configured"; return false; }
        if (!directory.is_absolute() || directory.filename() != L"doodesch.multiplayer" || directory.parent_path().filename() != L"data") {
            error = "Avatar data directory must be the exact absolute mod-owned data directory"; return false;
        }
        auto pins = std::make_unique<private_profile::DirectoryPins>();
        pins->verify(directory);
        const auto cache = directory / L"cache";
        pins->verify(cache, true);
        WIN32_FIND_DATAW found{};
        HANDLE scan = FindFirstFileW((cache / L"*.png").c_str(), &found);
        std::set<std::string> files;
        if (scan != INVALID_HANDLE_VALUE) {
            bool more = true;
            while (more) {
                if (cache_name(found.cFileName)) {
                    if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
                        error = "Avatar cache contains a non-regular content-addressed file"; break;
                    }
                    const std::wstring name(found.cFileName);
                    files.emplace(name.begin(), name.end());
                    if (files.size() > max_images) { error = "Avatar cache exceeds 128 immutable images"; break; }
                }
                more = FindNextFileW(scan, &found) != 0;
                if (!more && GetLastError() != ERROR_NO_MORE_FILES) error = win_error("Avatar cache enumeration");
            }
            if (!FindClose(scan)) { if (!error.empty()) error += "; "; error += win_error("Avatar cache FindClose"); }
            if (!error.empty()) return false;
        } else if (GetLastError() != ERROR_FILE_NOT_FOUND) { error = win_error("Avatar cache enumerate open"); return false; }
        state.data = directory; state.cache = cache; state.files = std::move(files); state.pins = std::move(pins);
        state.thread = GetCurrentThreadId(); error.clear(); return true;
    } catch (const std::exception& problem) { error = std::string("Avatar configuration failed: ") + problem.what(); return false; }
}
bool Provider::set_members(const std::vector<uint64_t>& members, std::string& error) {
    try {
    auto& state = *impl_;
    if (!state.on_thread(error)) return false;
    if (members.size() > max_members) { error = "Avatar roster exceeds 64 members"; return false; }
    const std::set<uint64_t> unique(members.begin(), members.end());
    if (unique.size() != members.size() || unique.count(0)) { error = "Avatar roster contains duplicate or zero member"; return false; }
    for (auto it = state.members.begin(); it != state.members.end();) {
        if (!unique.count(it->first)) it = state.members.erase(it); else ++it;
    }
    for (auto member : unique) state.members.try_emplace(member);
    error.clear(); return true;
    } catch (const std::exception& problem) { error = std::string("Avatar roster update failed: ") + problem.what(); return false; }
}
Result Provider::lookup(uint64_t member) const {
    std::string error; const auto& state = *impl_;
    if (!state.on_thread(error)) return failure(error);
    const auto found = state.members.find(member);
    return found == state.members.end() ? failure("Avatar member is not in the current roster") : found->second.result;
}
Result Provider::poll(uint64_t member, std::chrono::steady_clock::time_point now) {
    try {
        auto& state = *impl_; std::string error;
        if (!state.on_thread(error)) return failure(error);
        const auto found = state.members.find(member);
        if (found == state.members.end()) return failure("Avatar member is not in the current roster");
        auto& entry = found->second;
        if (entry.result.state != State::Pending || now < entry.next) return entry.result;
        entry.next = now + std::chrono::seconds(1);
        if (!state.bind(error)) { entry.result = failure(error); return entry.result; }
        const int image = state.api.large_avatar(state.api.friends, member);
        if (image == -1) {
            if (++entry.polls >= max_pending_polls) entry.result = failure("Avatar download not ready after 30 bounded polls");
            else entry.result.cause = "Steam avatar is still loading";
            return entry.result;
        }
        if (!image) { entry.result = {State::Missing, {}, "Steam user has no avatar", 0, 0}; return entry.result; }
        if (image < -1) { entry.result = failure("Steam avatar returned invalid image handle=" + std::to_string(image)); return entry.result; }
        const auto cached = state.images.find(image);
        if (cached != state.images.end()) { entry.result = cached->second; return entry.result; }
        if (state.images.size() >= max_images) { entry.result = failure("Avatar image cache reached 128 handles"); return entry.result; }
        Result result;
        uint32_t width = 0, height = 0;
        if (!state.api.image_size(state.api.utils, image, &width, &height)) result = failure("Steam GetImageSize rejected avatar image");
        else if (!width || !height || width > 512 || height > 512)
            result = failure("Steam avatar dimensions actual=" + std::to_string(width) + "x" + std::to_string(height) + " required=1..512 pixels per axis");
        else {
            std::vector<unsigned char> rgba(size_t(width)*height*4), png;
            if (!state.api.image_rgba(state.api.utils, image, rgba.data(), static_cast<int>(rgba.size()))) result = failure("Steam GetImageRGBA rejected avatar image");
            else {
                fit_rgba(width, height, rgba);
                if (!encode_png(width, height, rgba, png, error)) {
                    result = failure(error); state.images.emplace(image, result); entry.result = result; return result;
                }
                const auto digest = mt2mp::sha256(png);
                const auto filename = digest + ".png";
                if (state.files.size() >= max_images && !state.files.count(filename)) result = failure("Avatar disk cache reached 128 immutable images");
                else if (!publish_file(state.cache, std::wstring(filename.begin(), filename.end()), png, error)) result = failure(error);
                else { state.files.insert(filename); result = {State::Ready, "cache/" + filename, {}, width, height}; }
            }
        }
        state.images.emplace(image, result); entry.result = result; return result;
    } catch (const std::exception& problem) {
        auto result = failure(std::string("Avatar retrieval failed: ") + problem.what());
        const auto found = impl_->members.find(member);
        if (found != impl_->members.end()) found->second.result = result;
        return result;
    }
}
}
