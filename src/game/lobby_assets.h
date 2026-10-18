#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace mt2mp::game::lobby_assets {
enum class State { Unprepared, Ready, Error };
struct Result {
    State state = State::Unprepared;
    std::string relative_path, cause;
    uint32_t width = 0, height = 0;
};

// The same bounded read and close path is used with injected files and loaded PhysFS.
struct Api {
    std::function<void*(const char*)> open_read;
    std::function<int64_t(void*)> file_length;
    std::function<int64_t(void*, void*, uint64_t)> read_bytes;
    std::function<int(void*)> close;
    std::function<std::string()> last_error;
    bool ready() const { return open_read && file_length && read_bytes && close; }
};

// One startup owner; prepare outside rendering, then use memory-only lookups.
class Provider {
public:
    explicit Provider(Api api = {});
    ~Provider();
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
    bool prepare(const std::filesystem::path& exact_mod_data_directory);
    Result lookup(std::string_view key) const;
    bool prepared() const;
    const std::string& diagnostic() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
