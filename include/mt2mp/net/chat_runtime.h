#pragma once

#include "mt2mp/net/chat_core.h"
#include <mutex>

namespace mt2mp::net::chat {

struct Snapshot {
    bool connected = false;
    uint64_t session = 0;
    uint8_t local_peer = 0;
    std::vector<Entry> entries;
};

class Runtime {
public:
    static Runtime& instance();
    bool initialize(std::string& error);
    void tick();
    bool submit(const std::string& text, std::string& error, uint64_t* sequence = nullptr);
    Snapshot snapshot() const;

private:
    Runtime() = default;
    void receive(uint64_t sender, const uint8_t* bytes, uint32_t size);
    mutable std::mutex mutex_;
    Core core_;
    bool initialized_ = false;
    uint64_t session_ = 0;
    uint8_t local_ = 0;
};

} // namespace mt2mp::net::chat
