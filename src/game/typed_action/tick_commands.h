#pragma once
#include "action_session.h"
#include "../time_session.h"
#include <variant>
#include <algorithm>

namespace mt2mp::action_session::tick_commands {
// Both existing input streams keep their own peer sequence. Host order is shared.
struct TimeRequest {
    uint64_t peer=0,sequence=0,observed_revision=0;
    game::time_session::Intent intent=game::time_session::Intent::Pause;
    bool operator==(const TimeRequest&)const=default;
};
enum class SnapshotPurpose : uint8_t { Ordinary = 0, HostPeriodic = 1 };
struct SnapshotRequest {
    uint64_t peer=0,sequence=0,observed_revision=0;
    std::string nonce;
    SnapshotPurpose purpose=SnapshotPurpose::Ordinary;
    bool operator==(const SnapshotRequest&)const=default;
};
struct Input {
    uint64_t session=0;
    std::string snapshot_uuid;
    std::variant<Request,TimeRequest,SnapshotRequest> command;
    bool operator==(const Input&)const=default;
};
struct Assigned {
    uint64_t order=0;
    Input input;
    bool operator==(const Assigned&)const=default;
};
struct Batch {
    uint64_t session=0,tick=0,previous_order=0;
    uint8_t peer_mask=0;
    std::string snapshot_uuid;
    std::vector<Assigned> commands;
    bool operator==(const Batch&)const=default;
};
inline bool requires_evidence(const Input& input){
    const auto* r=std::get_if<Request>(&input.command);
    return r&&(((r->action==Action::SetNpcQuestList||r->action==Action::EditNpcQuest)&&ordered_quest_list(r->npc_quest_list))||(r->action==Action::ManagementCommand&&ordered_management(r->management))||(r->action==Action::ReleaseEdit&&game::release_edit::ordered_type(r->release_edit))||r->action==Action::RenameCharacter||r->action==Action::FireDeveloper||r->action==Action::CommitStartingpoint||monster_history(*r)||r->action==Action::EditGroup||r->action==Action::MoveSelection||r->action==Action::DuplicateSelection||r->action==Action::PlaceMonster||r->action==Action::MoveMonster||r->action==Action::RemoveMonster||r->action==Action::EditNpcProperty||r->action==Action::PlaceWall||r->action==Action::PlacePass||r->action==Action::TerrainStroke||
        ((r->action==Action::UndoBuilding||r->action==Action::RedoBuilding)&&
         !r->target_action_uuid.empty()&&(r->canonical_record_key=="terrain:"+r->target_action_uuid||
         r->canonical_record_key=="path:"+r->target_action_uuid||r->canonical_record_key=="groups:"+r->target_action_uuid||r->canonical_record_key=="selection:"+r->target_action_uuid||r->canonical_record_key=="duplicate:"+r->target_action_uuid)));
}
struct CommandEvidence {
    uint64_t order=0;
    std::string sha256;
    bool operator==(const CommandEvidence&)const=default;
};
inline bool evidence_digest(const std::string& value){
    return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
struct Receipt {
    uint64_t session=0,tick=0,last_order=0;
    std::string snapshot_uuid;
    size_t completed_commands=0;
    Outcome outcome=Outcome::Pending;
    bool tick_attempted=false,tick_completed=false;
    std::string cause;
    std::vector<CommandEvidence> command_evidence;
    bool operator==(const Receipt&)const=default;
};
struct CompletedAction {
    std::string action_uuid,snapshot_uuid;
    uint64_t session=0,order=0,tick=0,peer=0,sequence=0;
    uint8_t required_mask=0,ack_mask=0;
};

enum class Admission { Accepted,Duplicate,Rejected };
struct Config {
    Context context;
    uint64_t first_tick=1,initial_order=0;
    // Included in a future join checkpoint. Streams: gameplay, time, snapshot.
    std::array<std::array<uint64_t,5>,3> sequences{};
};
struct State {
    bool locked=false,consuming=false;
    uint64_t next_tick=0,next_assignment_tick=0,assigned_order=0,sealed_order=0,completed_order=0;
    size_t queued=0,in_flight=0;
    uint64_t evidence_tick=0,evidence_order=0;
    uint8_t evidence_ack_mask=0;
    std::string cause;
};
// This is an owned queue for the existing session controller, not a transport or
// another native executor. Wire admission and current-world guards remain callers.
class Queue {
public:
    static constexpr size_t batch_limit=64,history_limit=64,in_flight_limit=128;
    using Validate=std::function<bool(const Input&,std::string&)>;
    using Capacity=std::function<bool(const Batch&,std::string&)>;
    using Current=std::function<bool(std::string&)>;
    using Execute=std::function<Outcome(const Assigned&,std::string&)>;
    using Advance=std::function<Outcome(uint64_t,std::string&)>;
    Queue(Config,Validate);
    Queue(const Queue&)=delete;
    Queue& operator=(const Queue&)=delete;
    Admission offer(uint64_t authenticated_sender,const Input&,Assigned&,std::string&,const Capacity& = {});
    bool seal(Batch&,std::string&,const Capacity& = {});
    Admission admit(uint64_t authenticated_host,const Batch&,std::string&);
    // Same completed batch returns its retained receipt without invoking callbacks.
    bool consume(uint64_t boundary_tick,const Current&,const Execute&,const Advance&,Receipt&,std::string&);
    bool acknowledge(uint64_t authenticated_peer,const Receipt&,std::string&);
    bool bind_evidence(const Assigned&,const std::string& sha256,std::string&);
    State state()const;
    std::string retirement_diagnostic()const;
    bool can_retire_recovery(uint64_t completed_tick)const;
    bool recovery_boundary_matches(uint64_t tick,uint8_t mask)const;
    const std::deque<CompletedAction>& completed_actions()const noexcept{return completed_actions_;}
    bool current_execution(const Assigned&,Context&,uint64_t& tick,std::string&)const;
private:
    Config config_;
    Validate validate_;
    std::thread::id thread_;
    State state_;
    bool busy_=false;
    const Assigned* active_execution_=nullptr;
    uint64_t active_tick_=0;
    Receipt* active_receipt_=nullptr;
    std::vector<Assigned> queued_;
    std::deque<Assigned> history_;
    struct Slot {
        Batch batch;
        std::optional<Receipt> receipt;
        uint8_t acknowledgements=0;
        std::array<std::optional<Receipt>,5> peer_receipts{};
    };
    std::deque<Slot> in_flight_,completed_;
    std::deque<CompletedAction> completed_actions_;
    bool guard(std::string&);
    bool reject(const std::string&,std::string&,bool lock=false);
    bool input(const Input&,std::string&);
    bool capacity(const Capacity&,const Batch&,std::string&);
    void remember(const Assigned&);
    Slot* find(uint64_t tick);
    void retire();
};
}
