#pragma once
#include "vendor/nlohmann/json.hpp"
#include "region_action.h"
#include "region_property_action.h"
#include "price_action.h"
#include "terrain_action.h"
#include "path_action.h"
#include "network_action.h"
#include "network_pipe_action.h"
#include "npc_action.h"
#include "zone_action.h"
#include "monster_action.h"
#include "monster_quest_retirement.h"
#include "bug_fix_action.h"
#include "../tutorial_replication/startingpoint.h"
#include "../tutorial_replication/research.h"
#include "../developer_staff/shared_state.h"
#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace mt2mp::action_session {
constexpr uint8_t channel=9;
constexpr size_t max_payload=16384, max_transport_payload=65536, history_limit=64;
enum class Role:uint8_t { Host,Client };
enum class Action:uint8_t { RemoveSnapshotBuilding=1,PlaceBuilding,MoveBuilding,SetRegionPreset,TerrainStroke,SetRegionStyles=6,PlacePath=7,PlaceNetworkNode=8,PlaceNetworkPipe=9,UndoBuilding=10,RedoBuilding=11,EditRegionProperty=12,RemovePath=13,SetPrices=14,PlaceNPC=15,RemoveNPC=16,MoveNPC=17,CommitStartingpoint=18,StartStartingpoint=19,SetStartingpointWorkers=20,HireDeveloper=21,FireDeveloper=22,PlaceMonsterZone=23,SetBugFix=24,StartResearch=25,SetResearchWorkers=26,CommitResearch=27,PlaceMonster=28,MoveMonster=29,RemoveMonster=30 };
enum class Kind:uint8_t { Request=1,HostResult,ApplyAck,Completion,RequestRejected };
enum class Outcome:uint8_t { Pending,Confirmed,RejectedNoEffect,Unknown };
enum class AuthorityReadiness:uint8_t {Ready,Wait,Rejected};
struct BuildingState {
    std::string type,variant;
    float x=0,y=0,z=0;
    std::array<float,4> rotation{0,0,0,1};
    std::array<float,3> scale{1,1,1};
    bool alive=false;
    bool operator==(const BuildingState&) const = default;
};
struct Request {
    Action action=Action::RemoveSnapshotBuilding;
    uint64_t peer=0,sequence=0,host_revision=0;
    std::string action_uuid,snapshot_uuid,canonical_record_key,target_action_uuid;
    BuildingState building;
    RegionIntent region;
    RegionPropertyIntent region_property;
    PriceIntent price;
    TerrainIntent terrain;
    PathIntent path;
    PathDeleteIntent path_delete;
    NetworkIntent network;
    NetworkPipeIntent network_pipe;
    NpcIntent npc;
    ZoneIntent zone;
    MonsterIntent monster;
    MonsterMoveIntent monster_move;
    MonsterRemoveIntent monster_remove;
    BugFixIntent bug_fix;
    int32_t desired_workers=0;
    bool operator==(const Request&) const = default;
};
struct Ledger {
    int32_t cash=0,flat_day=0,record_present=0,record_day=0;
    int32_t record_cash=0,record_income=0,record_expense=0;
    std::array<int32_t,20> line_counts{},line_income{};
    bool operator==(const Ledger&) const = default;
};
struct Evidence {
    Ledger before,after;
    bool target_retired=false,undo_confirmed=false,selection_confirmed=false;
    bool retirement_confirmed=false;
    BuildingState object_before,object_after;
    RegionState region_before,region_after;
    RegionPropertyState property_before,property_after;
    PriceState price_before,price_after;
    TerrainState terrain_before,terrain_after;
    PathState path_before,path_after;
    NetworkState network_before,network_after;
    NpcState npc_before,npc_after;
    ZoneState zone_before,zone_after;
    MonsterState monster_before,monster_after;
    MonsterQuestRetirement monster_quests;
    std::optional<game::tutorial_replication::startingpoint::Row> startingpoint_before,startingpoint_after;
    std::optional<game::tutorial_replication::research::Row> research_before,research_after;
    std::optional<game::developer_staff::PoolState> developers_before,developers_after;
    std::optional<game::bug_authority::Row> bug_before,bug_after;
    std::string region_retired_sha256;
    bool operator==(const Evidence&) const = default;
};
struct Message {
    Kind kind=Kind::Request;
    uint64_t session=0,revision=0,ack_peer=0,required_mask=0;
    Request request;
    Outcome outcome=Outcome::Pending;
    Evidence evidence;
    std::string cause;
    bool operator==(const Message&) const = default;
};
struct Binding {
    uint64_t native_epoch=0,native_generation=0;
    bool operator==(const Binding&) const = default;
};
struct Context {
    uint64_t session=0,local_peer=0,host_peer=1;
    uint8_t peer_mask=0;
    Role role=Role::Client;
    std::string snapshot_uuid;
    bool operator==(const Context&) const = default;
};
class Session {
public:
    virtual ~Session()=default;
    // Must reject disconnected/not-loaded sessions and read actual local services.
    virtual bool current(Context&,std::string& cause)=0;
};
class Engine {
public:
    virtual ~Engine()=default;
    // Derive and validate the actual local native world on the bound game thread.
    virtual bool bind(const std::string& snapshot_uuid,Binding&,std::string& cause)=0;
    virtual bool current(const Binding&,const std::string& snapshot_uuid,std::string& cause)=0;
    virtual bool ledger(const Binding&,Ledger&,std::string& cause)=0;
    // Return RejectedNoEffect only if no native effect was attempted.
    virtual Outcome execute_remove(const Binding&,const Request&,std::string& cause)=0;
    // Host result is authority; independently computed client economy is never published.
    virtual Outcome apply_remove(const Binding&,const Request&,const Evidence& host,std::string& cause)=0;
    // Read the native operation receipt and current ledger without replaying execution.
    virtual bool observe_remove(const Binding&,const Request&,Evidence&,std::string& cause)=0;
    // Existing Remove adapters keep their entry points. Place and Move adapters
    // override these three methods and publish actual native object readback.
    virtual Outcome execute_action(const Binding& b,const Request& r,std::string& cause) {
        if(r.action==Action::RemoveSnapshotBuilding)return execute_remove(b,r,cause);
        cause="native action execution unsupported";return Outcome::RejectedNoEffect;
    }
    virtual Outcome apply_action(const Binding& b,const Request& r,const Evidence& host,std::string& cause) {
        if(r.action==Action::RemoveSnapshotBuilding)return apply_remove(b,r,host,cause);
        cause="native action application unsupported";return Outcome::RejectedNoEffect;
    }
    virtual bool observe_action(const Binding& b,const Request& r,Evidence& out,std::string& cause) {
        if(r.action==Action::RemoveSnapshotBuilding)return observe_remove(b,r,out,cause);
        cause="native action observation unsupported";return false;
    }
    virtual bool receive_authority_state(uint64_t,const uint8_t*,size_t,bool& handled,std::string&) {handled=false;return true;}
    virtual bool capture_host_authority_before(const Binding&,const Request&,const Ledger&,uint64_t,std::string&) {return true;}
    virtual bool publish_host_authority_before(const Message&,std::string&) {return true;}
    virtual AuthorityReadiness client_authority_before_ready(const Message&,uint64_t,std::string&) {return AuthorityReadiness::Ready;}
    virtual bool apply_client_authority_before(const Binding&,const Message&,uint64_t,std::string&) {return true;}
    // Retire retained operation bookkeeping without executing the action again.
    virtual bool retire(const Binding&,const Request&,std::string& cause)=0;
};
class Transport {
public:
    using Handler=std::function<void(uint64_t,const uint8_t*,size_t)>;
    virtual ~Transport()=default;
    virtual bool claim(Handler,std::string& cause)=0;
    virtual bool send(uint64_t target,const std::vector<uint8_t>&,std::string& cause)=0;
};
struct Status {
    bool started=false,locked=false,pending=false,last_packet_accepted=false;
    uint64_t host_revision=0,attempted=0,confirmed=0,failed=0,duplicates=0,suppressed_errors=0;
    uint8_t required_mask=0,ack_mask=0;
    Outcome outcome=Outcome::Pending;
    std::string action_uuid,cause;
};
using Logger=std::function<void(const char* level,const std::string& operation,const std::string& cause)>;
bool encode(const Message&,std::vector<uint8_t>&,std::string& cause);
bool decode(const uint8_t*,size_t,Message&,std::string& cause);
bool valid_bug_fix(const BugFixIntent&);
bool valid_bug_fix_effect(const Request&,const Evidence&);
bool valid_npc(const NpcIntent&);
bool valid_npc_effect(const Request&,const Evidence&);
class Controller {
public:
    bool submit_bug_fix(const std::string& action_uuid,const BugFixIntent&,uint64_t now,std::string& error);
    bool submit_developer(bool hire,const std::string& action_uuid,uint64_t now,std::string& error);
    bool submit_start_research(const std::string& action_uuid,const std::string& key,uint64_t now,std::string&);
    bool submit_research_workers(const std::string& action_uuid,const std::string& key,int32_t desired,uint64_t now,std::string&);
    bool submit_commit_research(const std::string& action_uuid,const std::string& key,uint64_t now,std::string&);
    bool submit_commit_startingpoint(const std::string& action_uuid,uint64_t now,std::string&);
    bool submit_startingpoint_workers(const std::string& action_uuid,int32_t desired,uint64_t now,std::string&);
    bool submit_start_startingpoint(const std::string& action_uuid,uint64_t now,std::string&);
    Controller(Session&,Engine&,Transport&,Logger,uint64_t ack_timeout_ms=10000);
    Controller(const Controller&)=delete;
    Controller& operator=(const Controller&)=delete;
    bool start(uint64_t now_ms,std::string& cause);
    bool submit_remove(const std::string& action_uuid,const std::string& canonical_record_key,uint64_t now_ms,std::string& cause);
    bool submit_place(const std::string& action_uuid,const BuildingState&,uint64_t now_ms,std::string& cause);
    bool submit_move(const std::string& action_uuid,const std::string& object_key,const BuildingState&,uint64_t now_ms,std::string& cause);
    bool submit_building_history(Action action,const std::string& action_uuid,const std::string& target_action_uuid,uint64_t now_ms,std::string& cause);
    bool submit_building_history(Action action,const std::string& action_uuid,const std::string& target_action_uuid,const std::string& object_key,uint64_t now_ms,std::string& cause);
    bool submit_network_pipe(const std::string& action_uuid,const NetworkPipeIntent&,uint64_t now_ms,std::string& cause);
    bool submit_network(const std::string& action_uuid,const NetworkIntent&,uint64_t now_ms,std::string& cause);
    bool submit_path_delete(const std::string& action_uuid,const PathDeleteIntent&,uint64_t now_ms,std::string& cause);
    bool submit_path(const std::string& action_uuid,const PathIntent&,uint64_t now_ms,std::string& cause);
    bool submit_terrain(const std::string& action_uuid,const TerrainIntent&,uint64_t now_ms,std::string& cause);
    bool submit_region(const std::string& action_uuid,const RegionIntent&,uint64_t now_ms,std::string& cause);
    bool submit_region_property(const std::string& action_uuid,const RegionPropertyIntent&,uint64_t now_ms,std::string& cause);
    bool submit_prices(const std::string& action_uuid,const PriceIntent&,uint64_t now_ms,std::string& cause);
    bool submit_remove_monster(const std::string&,const std::string&,const MonsterRemoveIntent&,uint64_t,std::string&);
    bool submit_move_monster(const std::string&,const std::string&,const MonsterMoveIntent&,uint64_t,std::string&);
    bool submit_monster(const std::string&,const MonsterIntent&,uint64_t,std::string&);
    bool submit_zone(const std::string& action_uuid,const ZoneIntent&,uint64_t now_ms,std::string& cause);
    bool submit_npc(const std::string& action_uuid,const NpcIntent&,uint64_t now_ms,std::string& cause);
    // NPC targets accept a confirmed Place UUID or a canonical saved-record key.
    bool submit_remove_npc(const std::string& action_uuid,const std::string& target,uint64_t now_ms,std::string& cause);
    bool submit_move_npc(const std::string& action_uuid,const std::string& target,const BuildingState& transform,uint64_t now_ms,std::string& cause);
    bool receive(uint64_t authenticated_sender,const uint8_t*,size_t,uint64_t now_ms,std::string& cause);
    bool tick(uint64_t now_ms,std::string& cause);
    Status status() const {return status_;}
    const Status& status_view() const noexcept {return status_;}
    bool can_retire_lobby() const;
    Context bound_context() const {return context_;}
    const Context& bound_context_view() const noexcept {return context_;}
    Binding bound_binding() const {return binding_;}
private:
    struct Cached {
        Message request,result,completion,ack;
        bool has_result=false,has_completion=false,has_ack=false;
        std::array<std::optional<Message>,5> peer_acks{};
    };
    Session& session_;Engine& engine_;Transport& transport_;Logger logger_;
    Context context_;Binding binding_;Status status_;
    std::thread::id thread_{};
    uint64_t timeout_,now_=0,deadline_=0,local_deadline_=0,local_sequence_=0,last_result_revision_=0;
    std::array<uint64_t,5> high_water_{};
    std::map<std::string,Cached> history_;
    std::deque<std::string> history_order_;
    std::optional<Message> pending_;
    std::optional<Message> authority_waiting_;
    std::optional<Message> local_request_;
    bool busy_=false,idle_session_loss_=false;
    std::string last_logged_error_;
    bool guard(std::string&);
    bool invoke(const std::function<bool()>&,std::string&);
    bool fail(const std::string&,std::string&,bool lock=false,bool session_loss=false);
    bool send(uint64_t,const Message&,std::string&);
    bool request(const Message&,uint64_t,std::string&);
    bool result(const Message&,std::string&);
    bool ack(const Message&,uint64_t,std::string&);
    bool completion(const Message&,std::string&);
    bool reject_request(const Message&,const std::string&,bool cache,std::string&);
    bool rejection(const Message&,std::string&);
    bool observe(const Request&,const Ledger&,Evidence&,std::string&);
    bool finish(Outcome,const std::string&,std::string&);
    bool submit(Request,uint64_t,std::string&);
    void remember(const std::string&,Cached);
};
}
