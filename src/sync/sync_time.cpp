#include "mt2mp/sync/sync_time.h"
#include "mt2mp/net/net_manager.h"
#include "mt2mp/net/packet_serializer.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::sync {

TimeSync& TimeSync::instance() {
    static TimeSync ts;
    return ts;
}

void TimeSync::send_time_state() {
    // TODO: Read actual game time from memory
    auto packet = net::PacketSerializer::make_time_sync(0, 0.0f, 1);
    net::NetManager::instance().broadcast_reliable(packet);
}

void TimeSync::on_remote_time_sync(uint32_t day, float fraction, uint8_t speed) {
    LOG_DEBUG("Remote time sync: day={} fraction={:.3f} speed={}", day, fraction, speed);
    // TODO: Adjust local game clock to match
}

void TimeSync::on_remote_pause() {
    LOG_INFO("Remote pause received");
    // TODO: Call game's pause function
}

void TimeSync::on_remote_unpause() {
    LOG_INFO("Remote unpause received");
    // TODO: Call game's unpause function
}

void TimeSync::on_remote_speed_change(uint8_t speed) {
    LOG_INFO("Remote speed change: {}", speed);
    // TODO: Call game's set speed function
}

} // namespace mt2mp::sync
