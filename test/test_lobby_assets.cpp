#include "game/lobby_assets.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/net/avatar_png.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>
#ifdef NDEBUG
#error Lobby asset tests require assertions
#endif

using namespace mt2mp::game::lobby_assets;
namespace {
constexpr std::array<const char*,9> entries{{"icons/server.png", "icons/map.png", "icons/person.png", "icons/talk.png",
    "icons/gears.png", "icons/plus.png", "icons/warn.png", "icons/lock.png", "icons/unlock.png"}};
constexpr std::array<const char*,9> keys{{"host", "world", "person", "chat", "settings", "plus", "warn", "lock", "unlock"}};
struct Fake {
    std::vector<unsigned char> bytes;
    std::map<std::string,int> opens;
    int lengths = 0, reads = 0, closes = 0;
    int64_t length_override = -2, read_override = -2;
    bool open_ok = true, close_ok = true, throw_read = false;
    bool current_handle_closed = false;
    std::string missing_entry;
    explicit Fake(std::vector<unsigned char> input) : bytes(std::move(input)) {}
    Api api() {
        return {
            [this](const char* path) -> void* {
                assert(std::find(entries.begin(), entries.end(), std::string(path)) != entries.end());
                ++opens[path]; current_handle_closed = false;
                return open_ok && missing_entry != path ? this : nullptr;
            },
            [this](void* file) -> int64_t { assert(file == this); ++lengths; return length_override == -2 ? int64_t(bytes.size()) : length_override; },
            [this](void* file, void* output, uint64_t count) -> int64_t {
                assert(file == this && count == bytes.size()); ++reads;
                if (throw_read) throw std::runtime_error("retained injected read cause");
                std::copy(bytes.begin(), bytes.end(), static_cast<unsigned char*>(output));
                return read_override == -2 ? int64_t(count) : read_override;
            },
            [this](void* file) { assert(file == this); ++closes; current_handle_closed = true; return close_ok ? 1 : 0; },
            [this] { return current_handle_closed ? "retained close diagnostic" : "retained read diagnostic"; }
        };
    }
    int open_count() const { int total = 0; for (const auto& item : opens) total += item.second; return total; }
};
std::filesystem::path directory(const std::filesystem::path& root, const char* name) {
    const auto data = root/name/"data"/"doodesch.multiplayer";
    std::filesystem::create_directories(data); return data;
}
void write(const std::filesystem::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream stream(path, std::ios::binary); stream.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    stream.close(); assert(stream);
}
std::vector<unsigned char> read(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    assert(!stream.bad()); return bytes;
}
void rejected(const std::filesystem::path& root, const char* scenario, Fake& fake, const char* cause) {
    Provider provider(fake.api());
    assert(!provider.prepare(directory(root, scenario)));
    const auto result = provider.lookup("host");
    assert(result.state == State::Error && result.relative_path.empty());
    assert(result.cause.find(cause) != std::string::npos);
    assert(provider.diagnostic().find(cause) != std::string::npos);
    const auto calls = fake.open_count();
    assert(!provider.prepare(root/"must-not-be-accessed"));
    assert(fake.open_count() == calls);
}
}

int main(int argc, char** argv) {
    assert(argc == 2);
    // Each invocation owns a fresh child so previous test output is never removed or repaired.
    const auto root = std::filesystem::absolute(argv[1])/mt2mp::private_profile::new_run_id();
    std::filesystem::create_directories(root);
    std::vector<unsigned char> png; std::string error;
    assert(mt2mp::net::avatars::encode_png(2, 1, {1,2,3,255,4,5,6,127}, png, error));
    Fake fake(png); Provider provider(fake.api());
    assert(!provider.prepared() && provider.lookup("host").state == State::Unprepared);
    for (const char* key : {"", "../host", "icons/server.png", "HOST", "arbitrary"}) {
        assert(provider.lookup(key).state == State::Error);
        assert(fake.open_count() == 0);
    }
    const auto data = directory(root,"shared");
    assert(provider.prepare(data) && provider.prepared() && provider.diagnostic().empty());
    const auto hash = mt2mp::sha256(png);
    const auto relative = "cache/" + hash + ".png";
    for (const auto* key : keys) {
        const auto result = provider.lookup(key);
        assert(result.state == State::Ready && result.cause.empty());
        assert(result.width == 2 && result.height == 1 && result.relative_path == relative);
    }
    for (const auto* entry : entries) assert(fake.opens[entry] == 1);
    assert(fake.reads == 9 && fake.lengths == 9 && fake.closes == 9);
    assert(provider.prepare(root/"ignored-after-startup"));
    for (int i = 0; i < 20; ++i) assert(provider.lookup("person").relative_path == relative);
    assert(fake.open_count() == 9 && fake.reads == 9);
    assert(read(data/relative) == png);
    size_t files = 0; for (const auto& file : std::filesystem::directory_iterator(data/"cache")) { assert(file.path().filename() == hash + ".png"); ++files; }
    assert(files == 1);
    const auto original_time = std::filesystem::last_write_time(data/relative) - std::chrono::hours(24);
    std::filesystem::last_write_time(data/relative, original_time);
    Fake same(png); Provider existing(same.api());
    assert(existing.prepare(data) && read(data/relative) == png);
    assert(std::filesystem::last_write_time(data/relative) == original_time);
    Fake degraded(png); degraded.missing_entry = "icons/person.png"; Provider partial(degraded.api());
    assert(!partial.prepare(directory(root,"partial")));
    assert(partial.lookup("host").state == State::Ready && partial.lookup("person").state == State::Error);
    assert(partial.lookup("person").relative_path.empty() && degraded.reads == 8 && degraded.closes == 8);

    const auto wrong_data = directory(root,"wrong-content"); std::filesystem::create_directories(wrong_data/"cache");
    auto corrupt = png; corrupt[40] ^= 1; write(wrong_data/relative,corrupt);
    Fake wrong(png); Provider wrong_cache(wrong.api()); assert(!wrong_cache.prepare(wrong_data));
    assert(wrong_cache.lookup("host").cause == "Lobby icon cache existing content differs");
    assert(read(wrong_data/relative) == corrupt);
    const auto short_data = directory(root,"wrong-length"); std::filesystem::create_directories(short_data/"cache");
    write(short_data/relative,{1}); Fake short_cache(png); Provider wrong_length(short_cache.api());
    assert(!wrong_length.prepare(short_data));
    assert(wrong_length.lookup("host").cause == "Lobby icon cache existing byte length differs");
    assert(read(short_data/relative) == std::vector<unsigned char>{1});

    Fake short_header({137,80,78,71,13,10,26,10}); rejected(root,"short-header",short_header,"PHYSFS_fileLength actual=8"); assert(short_header.reads == 0 && short_header.closes == 9);
    Fake oversized(png); oversized.length_override = 1048577; rejected(root,"oversized",oversized,"required=33..1048576"); assert(oversized.reads == 0 && oversized.closes == 9);
    Fake negative(png); negative.length_override = -1; rejected(root,"negative-length",negative,"PHYSFS_fileLength actual=-1"); assert(negative.reads == 0);
    Fake short_read(png); short_read.read_override = int64_t(png.size())-1; rejected(root,"short-read",short_read,"PHYSFS_readBytes actual="); assert(short_read.closes == 9);
    auto truncated = png; truncated.pop_back(); Fake truncated_png(truncated); rejected(root,"truncated-png",truncated_png,"truncated chunk");
    auto invalid = png; invalid[0] = 0; Fake invalid_png(invalid); rejected(root,"invalid-signature",invalid_png,"PNG signature");
    invalid = png; invalid[12] = 0; Fake invalid_ihdr(invalid); rejected(root,"invalid-ihdr",invalid_ihdr,"13-byte IHDR");
    invalid = png; invalid[16] = 1; Fake dimensions(invalid); rejected(root,"dimensions",dimensions,"required=1..2048 pixels");
    invalid = png; invalid[24] = 3; Fake format(invalid); rejected(root,"format",format,"IHDR format invalid");
    invalid = png; invalid[40] ^= 1; Fake crc(invalid); rejected(root,"crc",crc,"chunk CRC differs");
    Fake missing(png); missing.open_ok = false; rejected(root,"missing",missing,"PHYSFS_openRead failed entry=icons/server.png"); assert(missing.closes == 0);
    Fake close(png); close.close_ok = false; rejected(root,"close",close,"PHYSFS_close failed"); assert(close.closes == 9);
    Fake both(png); both.read_override = -1; both.close_ok = false; rejected(root,"read-close",both,"PHYSFS_readBytes actual=-1");
    Provider both_provider(both.api()); assert(!both_provider.prepare(directory(root,"read-close-detail")));
    assert(both_provider.lookup("host").cause.find("PHYSFS_close failed") != std::string::npos);
    assert(both_provider.lookup("host").cause.find("retained read diagnostic") != std::string::npos);
    assert(both_provider.lookup("host").cause.find("retained close diagnostic") != std::string::npos);
    Fake throwing(png); throwing.throw_read = true; throwing.close_ok = false;
    rejected(root,"read-exception",throwing,"retained injected read cause"); assert(throwing.closes == 9);
    Fake never(png); Provider invalid_directory(never.api()); assert(!invalid_directory.prepare(root/"wrong")); assert(never.open_count() == 0);
    const auto blocked_data = directory(root,"blocked-cache"); write(blocked_data/"cache",{1});
    Provider blocked(never.api()); assert(!blocked.prepare(blocked_data)); assert(never.open_count() == 0);
    std::cout << "Lobby asset allowlist, bounded PhysFS, PNG and immutable cache assertions passed\n";
}
