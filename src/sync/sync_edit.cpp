#include "mt2mp/sync/sync_edit.h"
#include "mt2mp/sync/conflict_resolver.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::sync {

EditSync& EditSync::instance() {
    static EditSync es;
    return es;
}

void EditSync::on_local_edit_begin(const char* reason) {
    if (!net::NetManager::instance().is_connected()) return;

    LOG_DEBUG("Local edit begin: {}", reason ? reason : "null");

    if (!Config::instance().is_host) {
        // Client: will need to send request to host
        // For now just log - actual blocking requires understanding edit internals
        blocking_ = false; // TODO: set true once we can actually block edits
    }
}

void EditSync::on_local_edit_end() {
    if (!net::NetManager::instance().is_connected()) return;

    LOG_DEBUG("Local edit end");

    if (Config::instance().is_host) {
        // Host: broadcast the edit to all clients
        game::EditOperation op;
        op.player_steam_id = net::SteamNetworking::instance().local_steam_id();
        op.op_type = net::EditOpType::GENERIC_EDIT;
        // TODO: Capture actual edit details from EditManager state

        auto packet = net::PacketSerializer::make_edit_approved(op);
        net::NetManager::instance().broadcast_reliable(packet);
    }
}

void EditSync::on_remote_edit_approved(const game::EditOperation& op) {
    LOG_INFO("Remote edit approved: seq={} type=0x{:04X}",
        op.sequence_id, static_cast<uint16_t>(op.op_type));
    // TODO: Replay the edit locally
    // This requires calling game functions to place/destroy entities
    blocking_ = false;
}

void EditSync::on_remote_edit_request(uint64_t sender, const game::EditOperation& op) {
    // Host only
    if (!Config::instance().is_host) return;

    auto result = ConflictResolver::instance().check(op);
    if (result == ConflictResult::Allowed) {
        // TODO: Apply the edit locally on host
        // Then broadcast approval
        auto packet = net::PacketSerializer::make_edit_approved(op);
        net::NetManager::instance().broadcast_reliable(packet);
    } else {
        auto packet = net::PacketSerializer::make_edit_rejected(
            op.sequence_id, ConflictResolver::result_string(result));
        net::NetManager::instance().send_reliable(sender, packet);
    }
}

void EditSync::process_queue() {
    while (!pending_requests_.empty()) {
        auto op = std::move(pending_requests_.front());
        pending_requests_.pop();
        on_remote_edit_request(op.player_steam_id, op);
    }
}

bool EditSync::is_blocking_local_edit() const {
    return blocking_;
}

} // namespace mt2mp::sync
