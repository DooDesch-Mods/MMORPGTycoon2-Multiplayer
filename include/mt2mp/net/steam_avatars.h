#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace mt2mp::net::avatars {
enum class State { Pending, Missing, Ready, Error };
struct Result {
    State state = State::Pending;
    std::string relative_path, cause;
    uint32_t width = 0, height = 0;
};

// Flat services also provide a deterministic seam for tests without contacting Steam.
struct Api {
    void* friends = nullptr;
    void* utils = nullptr;
    int (*large_avatar)(void*, uint64_t) = nullptr;
    bool (*image_size)(void*, int, uint32_t*, uint32_t*) = nullptr;
    bool (*image_rgba)(void*, int, unsigned char*, int) = nullptr;
    bool ready() const { return friends && utils && large_avatar && image_size && image_rgba; }
};

// One bound game-thread owner. Pass only the current confirmed Steam member roster.
class Provider {
public:
    explicit Provider(Api api = {});
    ~Provider();
    Provider(const Provider&) = delete;
    Provider& operator=(const Provider&) = delete;
    bool configure(const std::filesystem::path& exact_mod_data_directory, std::string& error);
    bool set_members(const std::vector<uint64_t>& members, std::string& error);
    Result poll(uint64_t member, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    // Cached read only: no Steam calls, filesystem access or encoding.
    Result lookup(uint64_t member) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
