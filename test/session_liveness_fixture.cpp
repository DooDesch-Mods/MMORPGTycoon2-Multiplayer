#include "mt2mp/net/local_session.h"
#include "mt2mp/net/sidecar_client.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/overlay/lobby_overlay.h"
#include "vendor/nlohmann/json.hpp"
#include <windows.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace std::chrono_literals;
using json = nlohmann::json;
static DWORD main_thread;
static bool overlay_called = false;
namespace mt2mp {
Config& Config::instance() { static Config config; return config; }
Logger& Logger::instance() { static Logger logger; return logger; }
void Logger::log(LogCategory, LogLevel, const char*, int, const std::string& message) {
    std::lock_guard lock(main_mutex_); std::cout << "log: " << message << std::endl;
}
}
namespace mt2mp::overlay {
LobbyOverlay& LobbyOverlay::instance() { static LobbyOverlay overlay; return overlay; }
void LobbyOverlay::set_visible(bool value) {
    if (GetCurrentThreadId() != main_thread) throw std::runtime_error("overlay invoked outside mainthread");
    visible_ = value; overlay_called = true;
}
}
static void require(bool valid, const char* reason) { if (!valid) throw std::runtime_error(reason); }
template<class Predicate> static void wait_for(Predicate predicate, int milliseconds, const char* reason) {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) throw std::runtime_error(reason);
        std::this_thread::sleep_for(10ms);
    }
}
int main(int argc, char** argv) {
    main_thread = GetCurrentThreadId();
    if (argc != 3) { std::cerr << "error: fixture requires port and mode\n"; return 2; }
    auto& transport = mt2mp::net::SidecarClient::instance();
    try {
        auto& config = mt2mp::Config::instance();
        config.local_test = true; config.peer_id = 2; config.session_id = 42; config.is_host = false;
        config.session_token = std::string(32, 'a'); config.sidecar_control_port = static_cast<uint16_t>(std::stoi(argv[1]));
        std::atomic<int> snapshot_calls{0}, action_calls{0};
        std::atomic<bool> wrong_thread{false};
        transport.register_handler(4, [&](uint64_t, const uint8_t*, uint32_t) { if (GetCurrentThreadId() != main_thread) wrong_thread = true; ++snapshot_calls; });
        transport.register_handler(6, [&](uint64_t, const uint8_t*, uint32_t) { if (GetCurrentThreadId() != main_thread) wrong_thread = true; ++action_calls; });
        require(mt2mp::net::start_local_session(), "session startup failed");
        wait_for([] { return mt2mp::net::local_session_connected(); }, 5000, "worker handshake unavailable without a mainthread tick");
        const std::string mode = argv[2];
        std::cout << "checkpoint: session-connected" << std::endl;
        if (mode == "stall") {
            std::this_thread::sleep_for(12000ms);
            require(mt2mp::net::local_session_connected(), "live host session ended during 12s mainthread pause");
            require(mt2mp::net::local_session_peers() == 1, "live host disappeared during mainthread pause");
            require(snapshot_calls == 0 && action_calls == 0, "engine handler invoked during mainthread pause");
            mt2mp::net::tick_local_session();
            require(snapshot_calls == 1 && action_calls == 1 && !wrong_thread, "mainthread dispatch did not deliver snapshot/action exactly once");
            mt2mp::net::tick_local_session();
            require(snapshot_calls == 1 && action_calls == 1, "engine packet replayed");
            std::this_thread::sleep_for(50ms);
            require(!overlay_called, "false session-ended overlay during healthy transport");
        } else {
            auto start = std::chrono::steady_clock::now();
            wait_for([] { return !mt2mp::net::local_session_connected(); }, 12500, "dead host/transport did not end while mainthread was paused");
            const auto measured = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
            if (mode == "silent" || mode == "replay") require(measured >= 9000 && measured < 12000, "host timeout measurement outside existing10s contract");
            if (mode == "eof" || mode == "overflow") require(measured < 2000, "TCP/queue failure was not observed promptly");
            const auto status = mt2mp::net::local_session_status();
            if (mode == "silent" || mode == "replay") require(status.find("Host nicht erreichbar") != std::string::npos, "dead host status lost its cause");
            if (mode == "eof") require(status.find("relay TCP EOF") != std::string::npos, "TCP EOF cause missing");
            if (mode == "overflow") require(status.find("receive queue limit") != std::string::npos, "queue overflow cause missing");
            require(!mt2mp::net::start_local_session(), "ended session was implicitly reset/reconnected");
            require(!mt2mp::net::local_session_connected(), "ended session became connected again");
            require(snapshot_calls == 0 && action_calls == 0, "native handler ran outside mainthread");
            require(!overlay_called, "worker changed overlay state");
            mt2mp::net::tick_local_session();
            require(overlay_called, "session-ended notice not applied on mainthread");
            std::cout << "measurement: " << json{{"end_ms", measured}, {"status", status}}.dump() << std::endl;
        }
        std::cout << "checkpoint: explicit-owner-shutdown" << std::endl;
        transport.shutdown();
        require(!transport.connected() && !transport.ready() && !mt2mp::net::local_session_connected(), "transport remained connected after shutdown");
        std::cout << json{{"passed", true}, {"mode", mode}, {"snapshot_callbacks", int(snapshot_calls)}, {"action_callbacks", int(action_calls)}, {"native_handlers_mainthread", !wrong_thread}, {"mainthread_pause_ms", mode == "stall" ? 12000 : 0}}.dump() << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: fixture failed cause=" << error.what() << std::endl;
        transport.shutdown(); return 1;
    }
}
