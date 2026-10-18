#include "lobby_assets.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/private_profile_paths.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <utility>
#include <vector>

namespace mt2mp::game::lobby_assets {
namespace {
constexpr size_t max_bytes = 1024 * 1024;
struct Asset { const char* key; const char* entry; };
constexpr std::array<Asset, 9> assets{{
    {"host", "icons/server.png"}, {"world", "icons/map.png"}, {"person", "icons/person.png"},
    {"chat", "icons/talk.png"}, {"settings", "icons/gears.png"}, {"plus", "icons/plus.png"},
    {"warn", "icons/warn.png"}, {"lock", "icons/lock.png"}, {"unlock", "icons/unlock.png"}
}};
Result failure(std::string cause) { return {State::Error, {}, std::move(cause), 0, 0}; }
void append(std::string& error, const std::string& cause) { if (!error.empty()) error += "; "; error += cause; }
std::string win_error(const char* operation, DWORD code = GetLastError()) {
    return std::string("Lobby icon cache ") + operation + " Win32=" + std::to_string(code);
}
bool bind(Api& api, std::string& error) {
    if (api.ready()) return true;
    if (api.open_read || api.file_length || api.read_bytes || api.close || api.last_error) {
        error = "Lobby icon PhysFS API is incomplete"; return false;
    }
    const auto module = GetModuleHandleW(L"libphysfs.dll");
    if (!module) { error = win_error("loaded libphysfs.dll unavailable"); return false; }
    const auto open = reinterpret_cast<void*(__cdecl*)(const char*)>(GetProcAddress(module, "PHYSFS_openRead"));
    const auto length = reinterpret_cast<int64_t(__cdecl*)(void*)>(GetProcAddress(module, "PHYSFS_fileLength"));
    const auto read = reinterpret_cast<int64_t(__cdecl*)(void*,void*,uint64_t)>(GetProcAddress(module, "PHYSFS_readBytes"));
    const auto close = reinterpret_cast<int(__cdecl*)(void*)>(GetProcAddress(module, "PHYSFS_close"));
    if (!open) { error = "Lobby icon missing PhysFS export PHYSFS_openRead"; return false; }
    if (!length) { error = "Lobby icon missing PhysFS export PHYSFS_fileLength"; return false; }
    if (!read) { error = "Lobby icon missing PhysFS export PHYSFS_readBytes"; return false; }
    if (!close) { error = "Lobby icon missing PhysFS export PHYSFS_close"; return false; }
    api = {open, length, read, close, {}};
    const auto error_code = reinterpret_cast<int(__cdecl*)()>(GetProcAddress(module, "PHYSFS_getLastErrorCode"));
    const auto error_text = reinterpret_cast<const char*(__cdecl*)(int)>(GetProcAddress(module, "PHYSFS_getErrorByCode"));
    if (error_code && error_text) api.last_error = [error_code,error_text] {
        const auto code = error_code();
        const auto text = error_text(code);
        return "PhysFS code=" + std::to_string(code) + (text ? std::string(" ") + text : "");
    };
    return true;
}
void retain_physfs_error(const Api& api, std::string& error) { if (api.last_error) append(error, api.last_error()); }
bool read_original(const Api& api, const char* entry, std::vector<unsigned char>& bytes, std::string& error) {
    void* file = api.open_read(entry);
    if (!file) { error = std::string("PHYSFS_openRead failed entry=") + entry; retain_physfs_error(api, error); return false; }
    bool ok = false;
    try {
        const auto length = api.file_length(file);
        if (length < 33 || length > static_cast<int64_t>(max_bytes)) {
            error = "PHYSFS_fileLength actual=" + std::to_string(length) + " required=33..1048576 bytes";
            if (length < 0) retain_physfs_error(api, error);
        } else {
            bytes.resize(static_cast<size_t>(length));
            const auto read = api.read_bytes(file, bytes.data(), static_cast<uint64_t>(bytes.size()));
            ok = read == length;
            if (!ok) {
                error = "PHYSFS_readBytes actual=" + std::to_string(read) + " required=" + std::to_string(length);
                retain_physfs_error(api, error);
            }
        }
    } catch (const std::exception& problem) { error = std::string("PhysFS icon read exception: ") + problem.what(); }
    try {
        if (api.close(file) == 0) { append(error, "PHYSFS_close failed"); retain_physfs_error(api, error); ok = false; }
    } catch (const std::exception& problem) { append(error, std::string("PHYSFS_close exception: ") + problem.what()); ok = false; }
    return ok;
}
uint32_t be32(const unsigned char* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
uint32_t crc32(const unsigned char* p, size_t size) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; ++i) {
        crc ^= p[i];
        for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
bool png_info(const std::vector<unsigned char>& bytes, uint32_t& width, uint32_t& height, std::string& error) {
    constexpr unsigned char signature[]{137,80,78,71,13,10,26,10};
    if (bytes.size() < 33 || bytes.size() > max_bytes || !std::equal(std::begin(signature), std::end(signature), bytes.begin())) {
        error = "Lobby icon PNG signature or byte length invalid"; return false;
    }
    if (be32(bytes.data()+8) != 13 || be32(bytes.data()+12) != 0x49484452u) {
        error = "Lobby icon PNG first chunk must be 13-byte IHDR"; return false;
    }
    width = be32(bytes.data()+16); height = be32(bytes.data()+20);
    if (!width || !height || width > 2048 || height > 2048) {
        error = "Lobby icon dimensions actual=" + std::to_string(width) + "x" + std::to_string(height) + " required=1..2048 pixels per axis"; return false;
    }
    const auto depth = bytes[24], color = bytes[25];
    const bool depth_ok = color == 0 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16) :
        color == 3 ? (depth == 1 || depth == 2 || depth == 4 || depth == 8) :
        (color == 2 || color == 4 || color == 6) && (depth == 8 || depth == 16);
    if (!depth_ok || bytes[26] || bytes[27] || bytes[28] > 1) { error = "Lobby icon PNG IHDR format invalid"; return false; }
    bool idat = false, palette = false, ended_idat = false;
    for (size_t offset = 8; offset < bytes.size();) {
        if (bytes.size()-offset < 12) { error = "Lobby icon PNG truncated chunk"; return false; }
        const auto length = be32(bytes.data()+offset), type = be32(bytes.data()+offset+4);
        if (length > bytes.size()-offset-12) { error = "Lobby icon PNG truncated chunk data"; return false; }
        if (crc32(bytes.data()+offset+4, size_t(length)+4) != be32(bytes.data()+offset+8+length)) {
            error = "Lobby icon PNG chunk CRC differs"; return false;
        }
        if (type == 0x49484452u && offset != 8) { error = "Lobby icon PNG duplicate IHDR"; return false; }
        if (type == 0x504c5445u) {
            if (idat || palette || !length || length > 768 || length % 3) { error = "Lobby icon PNG palette invalid"; return false; }
            palette = true;
        }
        if (type == 0x49444154u) {
            if (ended_idat || (color == 3 && !palette)) { error = "Lobby icon PNG IDAT order invalid"; return false; }
            if (length) idat = true;
        } else if (idat) ended_idat = true;
        if (type == 0x49454e44u) {
            if (length || !idat || offset+12 != bytes.size()) { error = "Lobby icon PNG IEND or image data invalid"; return false; }
            return true;
        }
        offset += size_t(length)+12;
    }
    error = "Lobby icon PNG missing IEND"; return false;
}
struct FileHandle {
    HANDLE value;
    explicit FileHandle(HANDLE file) : value(file) {}
    ~FileHandle() {
        if (value != INVALID_HANDLE_VALUE && !CloseHandle(value))
            std::fprintf(stderr, "error: Lobby icon exception cleanup CloseHandle Win32=%lu\n", GetLastError());
    }
};
bool close_file(FileHandle& file, std::string& error) {
    const auto value = file.value; file.value = INVALID_HANDLE_VALUE;
    if (CloseHandle(value)) return true;
    append(error, win_error("CloseHandle")); return false;
}
bool verify_file(const std::filesystem::path& path, const std::vector<unsigned char>& expected, const std::string& hash, std::string& error) {
    FileHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (file.value == INVALID_HANDLE_VALUE) { error = win_error("read open"); return false; }
    FILE_ATTRIBUTE_TAG_INFO attributes{}; LARGE_INTEGER size{};
    bool ok = GetFileInformationByHandleEx(file.value, FileAttributeTagInfo, &attributes, sizeof(attributes)) != 0;
    if (!ok) error = win_error("read attributes");
    else if (attributes.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) {
        error = "Lobby icon cache file is a reparse point or directory"; ok = false;
    }
    if (ok && !GetFileSizeEx(file.value, &size)) { error = win_error("read size"); ok = false; }
    if (ok && size.QuadPart != static_cast<LONGLONG>(expected.size())) { error = "Lobby icon cache existing byte length differs"; ok = false; }
    std::vector<unsigned char> actual(expected.size()); DWORD read = 0;
    if (ok && !ReadFile(file.value, actual.data(), static_cast<DWORD>(actual.size()), &read, nullptr)) { error = win_error("ReadFile"); ok = false; }
    if (ok && (read != actual.size() || actual != expected || mt2mp::sha256(actual) != hash)) {
        error = "Lobby icon cache existing content differs"; ok = false;
    }
    if (!close_file(file, error)) ok = false;
    return ok;
}
bool publish_file(const std::filesystem::path& cache, const std::vector<unsigned char>& bytes, const std::string& hash, std::string& error) {
    const auto name = hash + ".png";
    const auto target = cache / std::wstring(name.begin(), name.end());
    const auto attributes = GetFileAttributesW(target.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) return verify_file(target, bytes, hash, error);
    const auto missing = GetLastError();
    if (missing != ERROR_FILE_NOT_FOUND) { error = win_error("destination attributes", missing); return false; }
    std::array<unsigned char,16> nonce{};
    const auto random = BCryptGenRandom(nullptr, nonce.data(), ULONG(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if (random < 0) { error = "Lobby icon cache nonce BCrypt NTSTATUS=" + std::to_string(random); return false; }
    const auto hex = private_profile::hex(nonce.data(), nonce.size());
    const auto temporary = cache / (L".lobby-icon-" + std::wstring(hex.begin(), hex.end()) + L".tmp");
    FileHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.value == INVALID_HANDLE_VALUE) { error = win_error("temporary create"); return false; }
    DWORD written = 0;
    bool ok = WriteFile(file.value, bytes.data(), DWORD(bytes.size()), &written, nullptr) != 0;
    if (!ok) error = win_error("WriteFile");
    else if (written != bytes.size()) { error = "Lobby icon cache short write"; ok = false; }
    if (ok && !FlushFileBuffers(file.value)) { error = win_error("FlushFileBuffers"); ok = false; }
    if (!close_file(file, error)) ok = false;
    if (ok && MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) return verify_file(target, bytes, hash, error);
    if (ok) {
        const auto move = GetLastError();
        if (move == ERROR_ALREADY_EXISTS || move == ERROR_FILE_EXISTS) ok = verify_file(target, bytes, hash, error);
        else { error = win_error("publish rename", move); ok = false; }
    }
    if (!DeleteFileW(temporary.c_str())) { append(error, win_error("temporary cleanup")); ok = false; }
    return ok;
}
}

struct Provider::Impl {
    Api api;
    bool prepared = false, ready = false;
    std::string diagnostic;
    std::map<std::string, Result, std::less<>> results;
    std::unique_ptr<private_profile::DirectoryPins> pins;
    explicit Impl(Api input) : api(std::move(input)) {}
};
Provider::Provider(Api api) : impl_(std::make_unique<Impl>(std::move(api))) {}
Provider::~Provider() = default;
bool Provider::prepare(const std::filesystem::path& directory) {
    auto& state = *impl_;
    if (state.prepared) return state.ready;
    state.prepared = true;
    try {
        if (!directory.is_absolute() || directory.filename() != L"doodesch.multiplayer" || directory.parent_path().filename() != L"data")
            throw std::runtime_error("Lobby icon data directory must be the exact absolute mod-owned data directory");
        state.pins = std::make_unique<private_profile::DirectoryPins>();
        state.pins->verify(directory);
        const auto cache = directory / L"cache";
        state.pins->verify(cache, true);
        std::string error;
        if (!bind(state.api, error)) throw std::runtime_error(error);
        std::map<std::string, Result> images;
        for (const auto& asset : assets) {
            Result result;
            try {
                std::vector<unsigned char> bytes;
                uint32_t width = 0, height = 0; error.clear();
                if (!read_original(state.api, asset.entry, bytes, error) || !png_info(bytes, width, height, error)) result = failure(error);
                else {
                    const auto hash = mt2mp::sha256(bytes);
                    const auto found = images.find(hash);
                    if (found != images.end()) result = found->second;
                    else {
                        if (!publish_file(cache, bytes, hash, error)) result = failure(error);
                        else result = {State::Ready, "cache/" + hash + ".png", {}, width, height};
                        images.emplace(hash, result);
                    }
                }
            } catch (const std::exception& problem) { result = failure(std::string("Lobby icon preparation failed: ") + problem.what()); }
            if (result.state == State::Error) append(state.diagnostic, std::string(asset.key) + ": " + result.cause);
            state.results.emplace(asset.key, std::move(result));
        }
        state.ready = state.diagnostic.empty();
    } catch (const std::exception& problem) {
        state.diagnostic = problem.what();
        for (const auto& asset : assets) state.results.emplace(asset.key, failure(state.diagnostic));
    }
    return state.ready;
}
Result Provider::lookup(std::string_view key) const {
    const auto& state = *impl_;
    const auto allowed = std::find_if(assets.begin(), assets.end(), [&](const Asset& asset) { return key == asset.key; });
    if (allowed == assets.end()) return failure("Lobby icon key is not in the fixed allowlist");
    if (!state.prepared) return {State::Unprepared, {}, "Lobby icons have not been prepared", 0, 0};
    const auto found = state.results.find(key);
    return found == state.results.end() ? failure(state.diagnostic) : found->second;
}
bool Provider::prepared() const { return impl_->prepared; }
const std::string& Provider::diagnostic() const { return impl_->diagnostic; }
}
