#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include "mt2mp/net/relay_protocol.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

using namespace mt2mp::net::relay;
namespace {
constexpr size_t max_queue = 8 * 1024 * 1024;
struct Peer {
    SOCKET socket;
    Identity identity{};
    bool authenticated = false;
    bool dead = false;
    std::vector<uint8_t> receive, send;
    size_t sent = 0;
    std::chrono::steady_clock::time_point opened = std::chrono::steady_clock::now();
};
bool queue(Peer& peer, uint8_t type, uint8_t channel, const void* payload, uint32_t length) {
    auto bytes = frame(type, channel, payload, length);
    if (bytes.empty() || peer.send.size() - peer.sent + bytes.size() > max_queue) {
        fprintf(stderr, "error: peer=%llu send queue exceeds %zu bytes\n", peer.identity.peer, max_queue);
        peer.dead = true;
        return false;
    }
    if (peer.sent) { peer.send.erase(peer.send.begin(), peer.send.begin() + peer.sent); peer.sent = 0; }
    peer.send.insert(peer.send.end(), bytes.begin(), bytes.end());
    return true;
}
SOCKET listen_on(uint16_t port) {
    SOCKET socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket == INVALID_SOCKET) return socket;
    BOOL exclusive = TRUE;
    if (setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<char*>(&exclusive), sizeof(exclusive))) {
        fprintf(stderr, "error: exclusive bind option port=%u WSA=%d\n", port, WSAGetLastError()); closesocket(socket); return INVALID_SOCKET;
    }
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) || listen(socket, 4)) {
        fprintf(stderr, "error: bind/listen 127.0.0.1:%u WSA=%d\n", port, WSAGetLastError());
        closesocket(socket); return INVALID_SOCKET;
    }
    u_long nonblocking = 1;
    if (ioctlsocket(socket, FIONBIO, &nonblocking)) { fprintf(stderr, "error: listener nonblocking port=%u WSA=%d\n", port, WSAGetLastError()); closesocket(socket); return INVALID_SOCKET; }
    return socket;
}
void pump(Peer& peer, bool readable, bool writable) {
    if (writable && peer.sent < peer.send.size()) {
        int count = send(peer.socket, reinterpret_cast<const char*>(peer.send.data() + peer.sent), static_cast<int>(peer.send.size() - peer.sent), 0);
        if (count > 0) { peer.sent += count; if (peer.sent == peer.send.size()) { peer.send.clear(); peer.sent = 0; } }
        else if (count == 0 || WSAGetLastError() != WSAEWOULDBLOCK) { fprintf(stderr, "error: send peer=%llu WSA=%d\n", peer.identity.peer, WSAGetLastError()); peer.dead = true; }
    }
    if (readable) {
        uint8_t buffer[65536];
        int count = recv(peer.socket, reinterpret_cast<char*>(buffer), sizeof(buffer), 0);
        if (count > 0) {
            if (peer.receive.size() + count > max_queue) { fprintf(stderr, "error: receive buffer limit peer=%llu\n", peer.identity.peer); peer.dead = true; }
            else peer.receive.insert(peer.receive.end(), buffer, buffer + count);
        } else if (count == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
            fprintf(stderr, "%s: connection closed peer=%llu WSA=%d\n", count == 0 ? "info" : "error", peer.identity.peer, count == 0 ? 0 : WSAGetLastError()); peer.dead = true;
        }
    }
}
int server(uint16_t port, const std::string& token, uint64_t session) {
    SOCKET listener = listen_on(port);
    if (listener == INVALID_SOCKET) return 2;
    printf("info: RELAY_READY port=%u session=%llu\n", port, session);
    std::vector<Peer> peers;
    for (;;) {
        fd_set read, write; FD_ZERO(&read); FD_ZERO(&write); FD_SET(listener, &read);
        for (auto& peer : peers) { FD_SET(peer.socket, &read); if (!peer.send.empty()) FD_SET(peer.socket, &write); }
        timeval wait{0, 50000};
        if (select(0, &read, &write, nullptr, &wait) == SOCKET_ERROR) { fprintf(stderr, "error: server select WSA=%d\n", WSAGetLastError()); return 3; }
        if (FD_ISSET(listener, &read)) {
            SOCKET socket = accept(listener, nullptr, nullptr);
            if (socket != INVALID_SOCKET) {
                if (peers.size() >= 4) { fprintf(stderr, "warn: fifth peer refused\n"); closesocket(socket); }
                else { u_long value = 1; if (ioctlsocket(socket, FIONBIO, &value)) closesocket(socket); else peers.push_back(Peer{socket}); }
            }
        }
        for (auto& peer : peers) {
            pump(peer, FD_ISSET(peer.socket, &read), FD_ISSET(peer.socket, &write));
            if (!peer.authenticated && std::chrono::steady_clock::now() - peer.opened > std::chrono::seconds(10)) { fprintf(stderr, "error: handshake timeout required=10s\n"); peer.dead = true; }
            Header header; std::vector<uint8_t> payload;
            while (!peer.dead) {
                auto parsed = pop(peer.receive, header, payload);
                if (parsed == Parse::Incomplete) break;
                if (parsed == Parse::Invalid) { fprintf(stderr, "error: malformed relay frame\n"); peer.dead = true; break; }
                if (header.type == Hello && !peer.authenticated && payload.size() == sizeof(Identity)) {
                    Identity id; memcpy(&id, payload.data(), sizeof(id));
                    bool duplicate = std::any_of(peers.begin(), peers.end(), [&](const auto& other) { return other.authenticated && other.identity.peer == id.peer; });
                    if (id.session != session || !id.peer || id.peer > 4 || id.host != (id.peer == 1) || duplicate || memcmp(id.token, token.data(), 32)) {
                        fprintf(stderr, "error: invalid session identity or duplicate peer=%llu\n", id.peer); peer.dead = true; break;
                    }
                    peer.identity = id; peer.authenticated = true;
                    queue(peer, Ready, 0, nullptr, 0);
                    printf("info: PEER_READY peer=%llu host=%u\n", id.peer, id.host);
                } else if (header.type == Data && peer.authenticated && payload.size() >= sizeof(Address)) {
                    Address address; memcpy(&address, payload.data(), sizeof(address)); address.sender = peer.identity.peer;
                    memcpy(payload.data(), &address, sizeof(address));
                    for (auto& other : peers) if (&other != &peer && other.authenticated && !other.dead && (!address.target || address.target == other.identity.peer)) queue(other, Data, header.channel, payload.data(), static_cast<uint32_t>(payload.size()));
                } else if (header.type == Ping && peer.authenticated) queue(peer, Pong, 0, payload.data(), static_cast<uint32_t>(payload.size()));
                else if (header.type == Shutdown && peer.authenticated) peer.dead = true;
                else { fprintf(stderr, "error: unexpected relay message type=%u peer=%llu\n", header.type, peer.identity.peer); peer.dead = true; }
            }
        }
        for (auto& peer : peers) if (peer.dead) closesocket(peer.socket);
        peers.erase(std::remove_if(peers.begin(), peers.end(), [](const auto& peer) { return peer.dead; }), peers.end());
    }
}
int bridge(uint16_t relay_port, uint16_t control_port) {
    SOCKET upstream = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(relay_port); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (upstream == INVALID_SOCKET || connect(upstream, reinterpret_cast<sockaddr*>(&address), sizeof(address))) { fprintf(stderr, "error: connect relay=%u WSA=%d\n", relay_port, WSAGetLastError()); return 4; }
    u_long value = 1;
    if (ioctlsocket(upstream, FIONBIO, &value)) { fprintf(stderr, "error: relay nonblocking WSA=%d\n", WSAGetLastError()); closesocket(upstream); return 4; }
    SOCKET listener = listen_on(control_port);
    if (listener == INVALID_SOCKET) { closesocket(upstream); return 5; }
    printf("info: CONTROL_READY port=%u\n", control_port);
    SOCKET local = INVALID_SOCKET;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (local == INVALID_SOCKET && std::chrono::steady_clock::now() < deadline) {
        fd_set read; FD_ZERO(&read); FD_SET(listener, &read); timeval wait{0, 50000};
        if (select(0, &read, nullptr, nullptr, &wait) == SOCKET_ERROR) break;
        if (FD_ISSET(listener, &read)) local = accept(listener, nullptr, nullptr);
    }
    closesocket(listener);
    if (local == INVALID_SOCKET) { fprintf(stderr, "error: local DLL connect timeout required=15s\n"); closesocket(upstream); return 6; }
    if (ioctlsocket(local, FIONBIO, &value)) { fprintf(stderr, "error: local nonblocking WSA=%d\n", WSAGetLastError()); closesocket(local); closesocket(upstream); return 6; }
    Peer dll{local}, relay{upstream};
    while (!dll.dead && !relay.dead) {
        fd_set read, write; FD_ZERO(&read); FD_ZERO(&write);
        for (auto* peer : {&dll, &relay}) { FD_SET(peer->socket, &read); if (!peer->send.empty()) FD_SET(peer->socket, &write); }
        timeval wait{0, 50000};
        if (select(0, &read, &write, nullptr, &wait) == SOCKET_ERROR) { fprintf(stderr, "error: bridge select WSA=%d\n", WSAGetLastError()); break; }
        pump(dll, FD_ISSET(local, &read), FD_ISSET(local, &write));
        pump(relay, FD_ISSET(upstream, &read), FD_ISSET(upstream, &write));
        for (auto pair : {std::pair{&dll, &relay}, std::pair{&relay, &dll}}) {
            Header header; std::vector<uint8_t> payload;
            while (!pair.first->dead) {
                auto parsed = pop(pair.first->receive, header, payload);
                if (parsed == Parse::Incomplete) break;
                if (parsed == Parse::Invalid) { fprintf(stderr, "error: malformed bridge frame\n"); pair.first->dead = true; break; }
                queue(*pair.second, header.type, header.channel, payload.data(), static_cast<uint32_t>(payload.size()));
            }
        }
    }
    closesocket(local); closesocket(upstream); return 0;
}
}
int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    WSADATA data; int error = WSAStartup(MAKEWORD(2, 2), &data);
    if (error) { fprintf(stderr, "error: WSAStartup=%d\n", error); return 1; }
    std::string mode = "client", token; uint16_t relay = 47010, control = 47001; uint64_t session = 0;
    for (int i = 1; i < argc; ++i) {
        std::string argument = argv[i];
        if (i + 1 >= argc) { fprintf(stderr, "error: missing argument value %s\n", argument.c_str()); return 7; }
        std::string value = argv[++i];
        if (argument == "--mode") mode = value;
        else if (argument == "--relay-port" || argument == "--control-port") {
            unsigned long port = strtoul(value.c_str(), nullptr, 10);
            if (!port || port > 65535) return 7;
            (argument == "--relay-port" ? relay : control) = static_cast<uint16_t>(port);
        } else if (argument == "--token") token = value;
        else if (argument == "--session") session = _strtoui64(value.c_str(), nullptr, 10);
        else if (argument == "--relay-host" && value == "127.0.0.1") {}
        else { fprintf(stderr, "error: unsupported argument %s\n", argument.c_str()); return 7; }
    }
    if (mode == "server" && (token.size() != 32 || !session)) { fprintf(stderr, "error: server requires session and 32-byte token\n"); return 7; }
    int result = mode == "server" ? server(relay, token, session) : mode == "client" ? bridge(relay, control) : 7;
    WSACleanup(); return result;
}
