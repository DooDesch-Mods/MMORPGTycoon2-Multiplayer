#pragma once

#include "mt2mp/game/edit_operation.h"
#include "mt2mp/net/packet_types.h"
#include <queue>
#include <cstdint>

namespace mt2mp::sync {

class EditSync {
public:
    static EditSync& instance();

    // Called by EditManager hooks
    void on_local_edit_begin(const char* reason);
    void on_local_edit_end();

    // Called by net layer when remote edit arrives
    void on_remote_edit_approved(const game::EditOperation& op);
    void on_remote_edit_request(uint64_t sender, const game::EditOperation& op);

    // Host: process pending edit requests
    void process_queue();

    bool is_blocking_local_edit() const;

private:
    EditSync() = default;
    std::queue<game::EditOperation> pending_requests_;
    bool blocking_ = false;
};

} // namespace mt2mp::sync
