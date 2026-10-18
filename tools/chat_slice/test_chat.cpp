#include "mt2mp/net/chat_core.h"
#include "mt2mp/net/chat_runtime.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/overlay/chat_focus.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <deque>
#include <iostream>
#include <tuple>

using namespace mt2mp::net::chat;
struct Frame { uint8_t sender, target; std::vector<uint8_t> bytes; };
struct Fixture {
    std::array<Core, 5> nodes;
    std::deque<Frame> frames;
    std::string error;
    uint8_t count;
    explicit Fixture(uint8_t peers = 4) : count(peers) {
        const auto all = uint8_t((1u << count) - 1);
        for (uint8_t peer = 1; peer <= count; ++peer)
            nodes[peer].configure(99, peer, peer == 1 ? all : uint8_t(1 | (1u << (peer - 1))), 1);
    }
    Send send(uint8_t from) {
        return [this, from](const uint8_t* data, uint32_t size, uint64_t target) {
            frames.push_back({from, uint8_t(target), {data, data + size}}); return true;
        };
    }
    Packet decode_frame(const Frame& frame) {
        Packet packet; assert(decode(frame.bytes.data(), uint32_t(frame.bytes.size()), packet, error)); return packet;
    }
    bool deliver(const Frame& frame) {
        return nodes[frame.target].receive(frame.sender, frame.bytes.data(), uint32_t(frame.bytes.size()), send(frame.target), error);
    }
    void drain() {
        unsigned bound = 0;
        while (!frames.empty()) {
            assert(++bound < 1000); const auto frame = frames.front(); frames.pop_front(); assert(deliver(frame));
        }
    }
    void join() {
        for (uint8_t peer = 2; peer <= count; ++peer) assert(nodes[peer].tick(send(peer), error));
        drain(); for (uint8_t peer = 1; peer <= count; ++peer) assert(nodes[peer].connected());
    }
    void submit(uint8_t peer, const std::string& text) {
        assert(nodes[peer].submit(text, error)); assert(nodes[peer].tick(send(peer), error));
    }
    void equal_transcript() {
        using Row = std::tuple<uint64_t, uint8_t, uint64_t, uint64_t, std::string>;
        std::vector<Row> expected;
        for (uint8_t peer = 1; peer <= count; ++peer) {
            std::vector<Row> actual; uint64_t last = 0;
            for (const auto& entry : nodes[peer].entries()) {
                assert(entry.host_order > last); last = entry.host_order;
                actual.emplace_back(entry.host_order, entry.origin, entry.origin_admission, entry.sequence, entry.text);
                assert(entry.origin == peer ? entry.delivery == Delivery::Confirmed : entry.delivery == Delivery::Received);
            }
            if (peer == 1) expected = actual; else assert(actual == expected);
        }
    }
};

// Link the real Runtime body to controlled wire and log boundaries.
namespace runtime_fixture {
bool connected = true;
uint64_t session = 777;
mt2mp::net::SidecarHandler handler;
std::deque<Frame> frames;
std::vector<std::string> logs;
unsigned prepare_calls = 0;
bool prepare_result = false;
std::string prepare_error = "info: awaiting 1..3 Steam peers";
std::string last_prepare_error;
}
namespace mt2mp {
Logger& Logger::instance() { static Logger logger; return logger; }
void Logger::log(LogCategory, LogLevel, const char*, int, const std::string& message) { runtime_fixture::logs.push_back(message); }
}
namespace mt2mp::net::session_wire {
bool prepare(std::string& error) {
    ++runtime_fixture::prepare_calls;
    error = runtime_fixture::prepare_result ? std::string{} : runtime_fixture::prepare_error;
    // The real SessionWire owns exact, reason-change-only preparation logging.
    if (!error.empty() && error != runtime_fixture::last_prepare_error)
        runtime_fixture::logs.push_back("[SessionWire] " + error);
    runtime_fixture::last_prepare_error = error;
    return runtime_fixture::prepare_result;
}
bool connected() { return runtime_fixture::connected; }
uint64_t session_id() { return runtime_fixture::session; }
uint64_t local_peer() { return 2; }
uint8_t peer_mask() { return 3; }
bool claim_handler(uint8_t selected, SidecarHandler callback, std::string& error) {
    assert(selected == channel); assert(!runtime_fixture::handler);
    runtime_fixture::handler = std::move(callback); error.clear(); return true;
}
bool send(uint8_t selected, const uint8_t* bytes, uint32_t size, uint64_t target) {
    assert(selected == channel); assert(target == 1);
    runtime_fixture::frames.push_back({2, uint8_t(target), {bytes, bytes + size}}); return connected();
}
}

int main() {
    mt2mp::overlay::ChatFocus focus;
    assert(!focus.visible() && !focus.typing()); focus.toggle();
    assert(focus.visible() && focus.typing() && focus.take_focus_request());
    assert(!focus.take_focus_request()); focus.set_input_active(false); assert(!focus.typing());
    focus.request_input_focus(); assert(focus.take_focus_request()); focus.close(); assert(!focus.visible());
    std::string error;
    Packet packet; packet.kind = Kind::Request; packet.session = 99; packet.origin = 2;
    packet.sequence = 1; packet.conversation = 1; packet.challenge = packet.origin_admission = 1;
    packet.text = "Hallo ä";
    std::vector<uint8_t> bytes; assert(encode(packet, bytes, error));
    Packet decoded; assert(decode(bytes.data(), uint32_t(bytes.size()), decoded, error));
    assert(decoded.text == packet.text && decoded.origin_admission == 1);
    bytes[4] = 1; assert(!decode(bytes.data(), uint32_t(bytes.size()), decoded, error));
    assert(encode(packet, bytes, error)); bytes[57] = 0;
    assert(!decode(bytes.data(), uint32_t(bytes.size()), decoded, error));
    assert(!valid_text("\xc0\xaf") && !valid_text("line\nbreak") && !valid_text(std::string(241, 'x')));
    assert(!valid_text("\xed\xa0\x80") && !valid_text("\xf4\x90\x80\x80"));

    // Every origin submits before any request/echo is delivered.
    Fixture simultaneous; simultaneous.join();
    for (uint8_t peer = 1; peer <= 4; ++peer) simultaneous.submit(peer, "gleichzeitig " + std::to_string(peer));
    simultaneous.drain(); simultaneous.equal_transcript();
    assert(simultaneous.nodes[1].entries().size() == 4);

    // Host ACK alone must not confirm Client delivery to the other Clients.
    Fixture delayed; delayed.join(); delayed.submit(2, "Empfängerbestätigung");
    const Frame request = delayed.frames.front(); delayed.frames.pop_front(); assert(delayed.deliver(request));
    std::deque<Frame> held;
    while (!delayed.frames.empty()) {
        const auto frame = delayed.frames.front(); delayed.frames.pop_front();
        if (frame.target == 3 || frame.target == 4) held.push_back(frame); else assert(delayed.deliver(frame));
    }
    assert(delayed.nodes[2].entries()[0].host_order == 1);
    assert(delayed.nodes[2].entries()[0].delivery == Delivery::Queued);
    assert(delayed.nodes[2].entries()[0].expected_mask == 13);
    delayed.frames = held; delayed.drain(); delayed.equal_transcript();
    assert(delayed.nodes[2].entries()[0].ack_mask == 13);
    assert(delayed.deliver(request)); assert(delayed.frames.empty());
    const auto echoed = held.front(); assert(delayed.deliver(echoed)); delayed.drain();
    Packet changed_echo = delayed.decode_frame(echoed); changed_echo.text = "widersprüchliches Echo";
    assert(encode(changed_echo, bytes, error));
    assert(!delayed.nodes[echoed.target].receive(1, bytes.data(), uint32_t(bytes.size()), delayed.send(echoed.target), error));
    Packet conflict = delayed.decode_frame(request); conflict.text = "anderer Inhalt";
    assert(encode(conflict, bytes, error));
    assert(!delayed.nodes[1].receive(2, bytes.data(), uint32_t(bytes.size()), delayed.send(1), error));
    assert(delayed.nodes[1].entries().size() == 1);

    Fixture invalid; invalid.join();
    packet.origin = 3; assert(encode(packet, bytes, error));
    assert(!invalid.nodes[1].receive(2, bytes.data(), uint32_t(bytes.size()), invalid.send(1), error));
    packet.origin = 2; packet.session = 100; assert(encode(packet, bytes, error));
    assert(!invalid.nodes[1].receive(2, bytes.data(), uint32_t(bytes.size()), invalid.send(1), error));
    packet.session = 99; packet.challenge = 2; assert(encode(packet, bytes, error));
    assert(!invalid.nodes[1].receive(2, bytes.data(), uint32_t(bytes.size()), invalid.send(1), error));

    // Pre-world Leave/rehost uses admission identity, with no snapshot input.
    Fixture lifecycle(2); lifecycle.join(); lifecycle.submit(2, "alte Sitzung");
    const Frame stale_request = lifecycle.frames.front(); lifecycle.drain();
    assert(lifecycle.nodes[1].retire(lifecycle.send(1), error)); lifecycle.drain();
    assert(lifecycle.nodes[2].retired() && !lifecycle.nodes[2].connected());
    lifecycle.nodes[2].configure(99, 2, 3, 1); assert(lifecycle.nodes[2].retired());
    lifecycle.nodes[1].configure(99, 1, 3, 2); lifecycle.nodes[2].configure(99, 2, 3, 2);
    assert(lifecycle.nodes[2].tick(lifecycle.send(2), error));
    const auto new_hello = lifecycle.frames.front(); lifecycle.frames.pop_front(); assert(lifecycle.deliver(new_hello));
    const auto welcome = lifecycle.frames.front(); lifecycle.frames.pop_front();
    Packet old_welcome = lifecycle.decode_frame(welcome); old_welcome.challenge = 1;
    assert(encode(old_welcome, bytes, error));
    assert(!lifecycle.nodes[2].receive(1, bytes.data(), uint32_t(bytes.size()), lifecycle.send(2), error));
    assert(!lifecycle.nodes[2].connected()); assert(lifecycle.deliver(welcome));
    assert(!lifecycle.deliver(stale_request)); lifecycle.submit(2, "neue Sitzung"); lifecycle.drain();
    lifecycle.equal_transcript(); assert(lifecycle.nodes[1].entries().size() == 1);
    assert(lifecycle.nodes[2].entries()[0].origin_admission == 2);
    Packet stale_completion; stale_completion.kind = Kind::Completion; stale_completion.session = 99;
    stale_completion.origin = 2; stale_completion.sequence = 1; stale_completion.conversation = 1;
    stale_completion.challenge = stale_completion.origin_admission = stale_completion.host_order = 1;
    stale_completion.expected_mask = stale_completion.ack_mask = 1;
    assert(encode(stale_completion, bytes, error));
    assert(!lifecycle.nodes[2].receive(1, bytes.data(), uint32_t(bytes.size()), lifecycle.send(2), error));

    // Client-only rejoin uses the same Host conversation and a new origin challenge.
    assert(lifecycle.nodes[2].retire(lifecycle.send(2), error)); lifecycle.drain();
    lifecycle.nodes[2].configure(99, 2, 3, 3); assert(lifecycle.nodes[2].tick(lifecycle.send(2), error)); lifecycle.drain();
    assert(!lifecycle.deliver(new_hello));
    lifecycle.submit(2, "wieder beigetreten"); lifecycle.drain();
    assert(lifecycle.nodes[2].entries().size() == 1 && lifecycle.nodes[2].entries()[0].host_order == 2);
    assert(lifecycle.nodes[1].entries().size() == 2);

    Fixture timeout(3); timeout.join(); timeout.submit(2, "Warten");
    const auto timed_request = timeout.frames.front(); timeout.frames.pop_front(); assert(timeout.deliver(timed_request));
    while (!timeout.frames.empty()) {
        const auto frame = timeout.frames.front(); timeout.frames.pop_front();
        if (frame.target != 3) assert(timeout.deliver(frame));
    }
    assert(!timeout.nodes[1].tick(timeout.send(1), error, std::chrono::steady_clock::now() + std::chrono::seconds(11)));
    assert(error.find("10 Sekunden") != std::string::npos); timeout.drain();
    assert(timeout.nodes[2].entries()[0].delivery == Delivery::Failed);
    assert(timeout.nodes[2].entries()[0].ack_mask != timeout.nodes[2].entries()[0].expected_mask);

    Fixture failed(2); failed.join(); assert(failed.nodes[1].submit("Fehler", error));
    assert(!failed.nodes[1].tick([](const uint8_t*, uint32_t, uint64_t) { return false; }, error));
    assert(failed.nodes[1].retired() && failed.nodes[1].entries()[0].delivery == Delivery::Failed);
    Fixture disconnected(2); disconnected.join(); disconnected.submit(2, "Abbruch");
    disconnected.nodes[2].configure(0, 0, 0, 1);
    assert(disconnected.nodes[2].entries()[0].delivery == Delivery::Failed && !disconnected.nodes[2].connected());
    assert(!disconnected.nodes[2].tick(disconnected.send(2), error)); assert(!error.empty());
    assert(disconnected.nodes[2].tick(disconnected.send(2), error)); assert(error.empty());
    Fixture bounded(2); bounded.join();
    for (unsigned i = 0; i < 16; ++i) assert(bounded.nodes[2].submit("x", error));
    assert(!bounded.nodes[2].submit("voll", error)); assert(bounded.nodes[2].tick(bounded.send(2), error)); bounded.drain();
    for (unsigned i = 0; i < 150; ++i) { bounded.submit(2, "x"); bounded.drain(); }
    assert(bounded.nodes[2].entries().size() == 100); bounded.equal_transcript();

    // Exercise real Runtime admission idempotency and fresh submit-time wire check.
    auto& runtime = Runtime::instance(); assert(runtime.initialize(error));
    assert(!runtime.submit("vor Anmeldung", error));
    runtime.tick(); assert(runtime_fixture::prepare_calls == 0);
    runtime.set_lobby_admitted(true); runtime.set_lobby_admitted(true);
    assert(runtime_fixture::prepare_calls == 0);
    runtime.tick(); runtime.tick();
    assert(runtime_fixture::prepare_calls == 2);
    assert(runtime_fixture::frames.empty() && !runtime.snapshot().connected);
    assert(runtime_fixture::logs.back() == "[SessionWire] info: awaiting 1..3 Steam peers");
    const auto waiting_logs = runtime_fixture::logs.size();
    runtime_fixture::prepare_error = "info: awaiting Host session metadata";
    runtime.tick(); runtime.tick();
    assert(runtime_fixture::prepare_calls == 4 && runtime_fixture::frames.empty());
    assert(runtime_fixture::logs.size() == waiting_logs + 1);
    assert(runtime_fixture::logs.back() == "[SessionWire] info: awaiting Host session metadata");
    runtime_fixture::prepare_error = "Steam Host session metadata differs from physical membership/version/build";
    runtime.tick(); runtime.tick();
    assert(runtime_fixture::prepare_calls == 6 && runtime_fixture::frames.empty());
    assert(!runtime.snapshot().connected);
    assert(runtime_fixture::logs.size() == waiting_logs + 2);
    assert(runtime_fixture::logs.back() == "[SessionWire] " + runtime_fixture::prepare_error);
    // A true wire failure retires the real transport. Re-admit only after its owner resets it.
    runtime.set_lobby_admitted(false); runtime.tick();
    assert(runtime_fixture::prepare_calls == 6);
    runtime.set_lobby_admitted(true);
    runtime_fixture::prepare_result = true;
    runtime_fixture::connected = false;
    runtime.tick(); assert(runtime_fixture::prepare_calls == 7);
    assert(runtime_fixture::frames.empty() && !runtime.snapshot().connected);
    runtime_fixture::connected = true;
    runtime.tick(); assert(runtime_fixture::prepare_calls == 8);
    assert(runtime_fixture::frames.size() == 1);
    Packet hello; const auto first_hello = runtime_fixture::frames.front(); runtime_fixture::frames.clear();
    assert(decode(first_hello.bytes.data(), uint32_t(first_hello.bytes.size()), hello, error));
    assert(hello.kind == Kind::Hello && hello.challenge == 2);
    Packet welcome_packet; welcome_packet.kind = Kind::Welcome; welcome_packet.session = 777;
    welcome_packet.origin = 1; welcome_packet.challenge = 2; welcome_packet.conversation = 1;
    assert(encode(welcome_packet, bytes, error)); runtime_fixture::handler(1, bytes.data(), uint32_t(bytes.size()));
    assert(runtime.snapshot().connected); assert(runtime.submit("im Lobby-Menü", error));
    runtime.set_lobby_admitted(false); const auto retire_count = runtime_fixture::frames.size();
    runtime.set_lobby_admitted(false); assert(runtime_fixture::frames.size() == retire_count);
    assert(!runtime.snapshot().connected && runtime.snapshot().retired);
    const auto admitted_prepare_calls = runtime_fixture::prepare_calls;
    runtime.tick(); assert(runtime_fixture::prepare_calls == admitted_prepare_calls);
    assert(runtime.snapshot().entries[0].delivery == Delivery::Failed);
    runtime_fixture::frames.clear(); runtime.set_lobby_admitted(true); runtime.tick();
    assert(runtime_fixture::frames.size() == 1); const auto second_hello = runtime_fixture::frames.front();
    assert(decode(second_hello.bytes.data(), uint32_t(second_hello.bytes.size()), hello, error)); assert(hello.challenge == 3);
    runtime_fixture::handler(1, bytes.data(), uint32_t(bytes.size())); // Prior Welcome has old challenge.
    assert(!runtime.snapshot().connected);
    welcome_packet.challenge = 3; assert(encode(welcome_packet, bytes, error));
    runtime_fixture::handler(257, bytes.data(), uint32_t(bytes.size())); assert(!runtime.snapshot().connected);
    runtime_fixture::handler(1, bytes.data(), uint32_t(bytes.size())); assert(runtime.snapshot().connected);
    runtime_fixture::connected = false;
    assert(!runtime.submit("nach Verbindungsende", error)); assert(!runtime.snapshot().connected);
    std::cout << "chat_ordered: actual core/protocol/runtime scenarios passed\n";
}
