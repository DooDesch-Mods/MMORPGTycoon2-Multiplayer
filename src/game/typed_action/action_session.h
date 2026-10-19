#pragma once
#include "vendor/nlohmann/json.hpp"
#include "region_action.h"
#include "region_property_action.h"
#include "price_action.h"
#include "management_action.h"
#include "network_remove_action.h"
#include "network_history_action.h"
#include "logo_action.h"
#include "general_project_action.h"
#include "release_edit_action.h"
#include "release_cycle_action.h"
#include "gm_policy_action.h"
#include "game_rules_action.h"
#include "intervention_action.h"
#include "terrain_action.h"
#include "path_action.h"
#include "network_action.h"
#include "network_pipe_action.h"
#include "npc_action.h"
#include "npc_property_action.h"
#include "prop_text_action.h"
#include "district_action.h"
#include "group_action.h"
#include "duplicate_action.h"
#include "scenery_stroke_action.h"
#include "group_object_action.h"
#include "boss_placement_action.h"
#include "world_path_action.h"
#include "boss_phase_action.h"
#include "service_membership_action.h"
#include "zone_action.h"
#include "monster_action.h"
#include "monster_quest_retirement.h"
#include "npc_quest_action.h"
#include "npc_quest_list_action.h"
#include "character_rename_action.h"
#include "character_membership_action.h"
#include "ability_edit_action.h"
#include "ability_membership_action.h"
#include "costume_edit_action.h"
#include "dungeon_config_action.h"
#include "dungeon_sound_action.h"
#include "dungeon_palette_action.h"
#include "dungeon_gizmo_action.h"
#include "dungeon_light_action.h"
#include "building_status_action.h"
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
constexpr size_t max_payload=60000, max_transport_payload=65536, history_limit=64;
enum class Role:uint8_t { Host,Client };
enum class Action:uint8_t { RemoveSnapshotBuilding=1,PlaceBuilding,MoveBuilding,SetRegionPreset,TerrainStroke,SetRegionStyles=6,PlacePath=7,PlaceNetworkNode=8,PlaceNetworkPipe=9,UndoBuilding=10,RedoBuilding=11,EditRegionProperty=12,RemovePath=13,SetPrices=14,PlaceNPC=15,RemoveNPC=16,MoveNPC=17,CommitStartingpoint=18,StartStartingpoint=19,SetStartingpointWorkers=20,HireDeveloper=21,FireDeveloper=22,PlaceMonsterZone=23,SetBugFix=24,StartResearch=25,SetResearchWorkers=26,CommitResearch=27,PlaceMonster=28,MoveMonster=29,RemoveMonster=30,SetNpcQuestDestination=31,EditNpcProperty=40,EditPropText=41,ReorderBossPhase=42,EditServiceMembership=43,MoveSelection=44,EditDistrictText=45,MoveDistrictTitle=46,EditGroup=47,DuplicateSelection=48,SceneryBrushStroke=49,SceneryEraseStroke=50,MoveGroup=51,RemoveGroup=52,PlaceBoss=53,PlaceWall=54,PlacePass=55,ManagementCommand=128,RemoveNetworkObject=129,SetLogo=130,GeneralProject=133,ReleaseEdit=134,ReleaseCycle=135,GmPolicy=136,GameRules=137,Intervention=138,SetNpcQuestList=32,EditNpcQuest=33,RenameCharacter=80,SetCharacterHealth=81,SetCharacterMana=82,SetCharacterRage=83,SetCharacterSpeed=84,EditCharacterAbility=85,EditCharacterCostume=86,EditCharacterMembership=87,EditAbilityMembership=88,SetBuildingStatus=100,ConfigureDungeonInstances=101,SetDungeonAtmosphere=103,SetDungeonPalette=104,EditDungeonGizmo=105,SetDungeonLighting=102 };
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
#include "selection_move_action.inc"
struct PropTextState {
    BuildingState object;
    bool scenery=false;
    std::string name,description;
    bool dungeon=false;
    std::string dungeon_name,dungeon_region_name;
    bool operator==(const PropTextState&)const=default;
};
struct Request {
    Action action=Action::RemoveSnapshotBuilding;
    uint64_t peer=0,sequence=0,host_revision=0;
    std::string action_uuid,snapshot_uuid,canonical_record_key,target_action_uuid;
    BuildingState building;
    RegionIntent region;
    RegionPropertyIntent region_property;
    PriceIntent price;
    ManagementIntent management;
    NetworkRemoveIntent network_remove;
    LogoIntent logo;
    GeneralProjectIntent general_project;
    ReleaseEditIntent release_edit;
    ReleaseCycleIntent release_cycle;
    GmPolicyIntent gm_policy;
    GameRulesIntent game_rules;
    InterventionIntent intervention;
    TerrainIntent terrain;
    PathIntent path;
    PathDeleteIntent path_delete;
    NetworkIntent network;
    NetworkPipeIntent network_pipe;
    NpcIntent npc;
    NpcPropertyIntent npc_property;
    PropTextIntent prop_text;
    BossPhaseIntent boss_phase;
    ServiceMembershipIntent service_membership;
    SelectionMoveIntent selection_move;
    DistrictTextIntent district_text;
    DistrictTitleIntent district_title;
    GroupIntent group;
    DuplicateIntent duplicate;
    SceneryStrokeIntent scenery_stroke;
    GroupObjectIntent group_object;
    BossPlacementIntent boss_placement;
    WorldPathIntent world_path;
    ZoneIntent zone;
    MonsterIntent monster;
    MonsterMoveIntent monster_move;
    MonsterRemoveIntent monster_remove;
    NpcQuestIntent npc_quest;
    NpcQuestListIntent npc_quest_list;
    CharacterRenameIntent character_rename;
    CharacterMembershipIntent character_membership;
    DungeonLightIntent dungeon_light;
    DungeonGizmoIntent dungeon_gizmo;
    DungeonPaletteIntent dungeon_palette;
    DungeonSoundIntent dungeon_sound;
    DungeonConfigIntent dungeon_config;
    CostumeEditIntent costume_edit;
    AbilityEditIntent ability_edit;
    AbilityMembershipIntent ability_membership;
    BuildingStatusIntent building_status;
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
    std::optional<ManagementState> management_before,management_after;
    std::optional<NetworkRemoval> network_removal_before,network_removal_after;
    std::optional<NetworkHistoryProof> network_history;
    std::optional<LogoState> logo_before,logo_after;
    std::optional<GeneralProjectProof> general_project_before,general_project_after;
    std::optional<ReleaseEditProof> release_edit_before,release_edit_after;
    std::optional<ReleaseCycleProof> release_cycle_before,release_cycle_after;
    std::optional<GmPolicyProof> gm_policy_before,gm_policy_after;
    std::optional<GameRulesProof> game_rules_before,game_rules_after;
    std::optional<InterventionProof> intervention_before,intervention_after;
    std::optional<game::release_cycle::Publication> release_cycle_value;
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
    std::optional<NpcPropertyState> npc_property_before,npc_property_after;
    std::optional<PropTextState> prop_text_before,prop_text_after;
    std::optional<BossPhaseState> boss_phase_before,boss_phase_after;
    std::optional<ServiceMembershipState> service_membership_before,service_membership_after;
    std::optional<SelectionMoveState> selection_move_before,selection_move_after;
    std::optional<DistrictState> district_before,district_after;
    std::optional<GroupState> group_before,group_after;
    std::optional<DuplicateState> duplicate_before,duplicate_after;
    std::vector<DuplicateCash> duplicate_cash;
    std::optional<SceneryStrokeState> scenery_stroke_before,scenery_stroke_after;
    std::optional<SceneryStrokeEffect> scenery_stroke_result;
    std::optional<GroupObjectState> group_object_before,group_object_after;
    std::optional<BossPlacementState> boss_placement_before,boss_placement_after;
    std::optional<WorldPathState> world_path_before,world_path_after;
    ZoneState zone_before,zone_after;
    MonsterState monster_before,monster_after;
    MonsterQuestRetirement monster_quests;
    NpcQuestState quest_before,quest_after;
    NpcQuestListState quest_list_before,quest_list_after;
    CharacterRenameState character_rename_before,character_rename_after;
    CharacterMembershipState character_membership_before,character_membership_after;
    DungeonLightState dungeon_light_before,dungeon_light_after;
    DungeonGizmoState dungeon_gizmo_before,dungeon_gizmo_after;
    DungeonPaletteState dungeon_palette_before,dungeon_palette_after;
    DungeonSoundState dungeon_sound_before,dungeon_sound_after;
    DungeonConfigState dungeon_config_before,dungeon_config_after;
    CostumeEditState costume_edit_before,costume_edit_after;
    AbilityEditState ability_edit_before,ability_edit_after;
    AbilityMembershipState ability_membership_before,ability_membership_after;
    BuildingStatusState building_status_before,building_status_after;
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
bool dungeon_config_effect(const Request&,const Ledger&,const Ledger&,const DungeonConfigState&);
bool dungeon_light_wire_fits(const Request&,const DungeonLightState&,const Ledger&,std::string&);
bool dungeon_gizmo_wire_fits(const Request&,const DungeonGizmoState&,const Ledger&,std::string&);
bool dungeon_palette_wire_fits(const Request&,const DungeonPaletteState&,const Ledger&,std::string&);
bool character_membership_wire_fits(const Request&,const CharacterMembershipState&,const Ledger&,std::string&);
bool ability_membership_wire_fits(const Request&,const AbilityMembershipState&,const Ledger&,std::string&);
bool dungeon_sound_wire_fits(const Request&,const DungeonSoundState&,const Ledger&,std::string&);
bool dungeon_config_wire_fits(const Request&,const DungeonConfigState&,const Ledger&,std::string&);
bool costume_edit_wire_fits(const Request&,const CostumeEditState&,const Ledger&,std::string&);
bool ability_edit_wire_fits(const Request&,const AbilityEditState&,const Ledger&,std::string&);
bool decode(const uint8_t*,size_t,Message&,std::string& cause);
bool valid_bug_fix(const BugFixIntent&);
bool valid_bug_fix_effect(const Request&,const Evidence&);
bool valid_boss_placement(const BossPlacementIntent&);
bool valid_boss_placement_effect(const Request&,const Evidence&);
bool valid_world_path_effect(const Request&,const Evidence&);
bool boss_placement_wire_fits(const Request&,const Evidence&,std::string&);
bool valid_group_object(bool,const GroupObjectIntent&);
bool valid_group_object_effect(const Request&,const Evidence&);
bool group_object_wire_fits(const Request&,const Evidence&,std::string&);
bool valid_scenery_stroke(const SceneryStrokeIntent&);
bool valid_scenery_stroke_effect(const Request&,const Evidence&);
bool scenery_stroke_wire_fits(const Request&,const Evidence&,std::string&);
bool valid_duplicate(const DuplicateIntent&);
bool valid_duplicate_effect(const Request&,const Evidence&);
bool duplicate_wire_fits(const Request&,const DuplicateState&,const Ledger&,std::string&);
bool valid_group(const GroupIntent&);
bool valid_group_effect(const Request&,const Evidence&);
bool valid_group_history_effect(const Request&,const Evidence&);
bool group_history_wire_fits(const Request&,const GroupState&,const GroupState&,const Ledger&,std::string&);
bool valid_duplicate_history_effect(const Request&,const Evidence&);
bool duplicate_history_wire_fits(const Request&,const DuplicateState&,const DuplicateState&,const std::vector<DuplicateCash>&,const Ledger&,std::string&);
bool group_wire_fits(const Request&,const GroupState&,const Ledger&,std::string&);
bool valid_district_title(const DistrictTitleIntent&);
bool valid_district_title_effect(const Request&,const Evidence&);
bool district_title_wire_fits(const Request&,const DistrictState&,const Ledger&,std::string&);
bool valid_district_text(const DistrictTextIntent&);
bool valid_district_effect(const Request&,const Evidence&);
bool district_wire_fits(const Request&,const DistrictState&,const Ledger&,std::string&);
bool valid_selection_move(const SelectionMoveIntent&);
bool valid_selection_move_effect(const Request&,const Evidence&);
bool selection_move_wire_fits(const Request&,const Ledger&,std::string&);
bool valid_service_membership_effect(const Request&,const Evidence&);
bool service_membership_wire_fits(const Request&,const ServiceMembershipState&,const Ledger&,std::string&);
bool valid_boss_phase(const BossPhaseIntent&);
bool valid_boss_phase_effect(const Request&,const Evidence&);
bool valid_prop_text(const PropTextIntent&);
bool valid_prop_text_effect(const Request&,const Evidence&);
bool valid_npc_property(const NpcPropertyIntent&);
bool valid_npc_property_effect(const Request&,const Evidence&);
bool valid_npc(const NpcIntent&);
bool valid_npc_effect(const Request&,const Evidence&);
class Controller {
public:
    bool submit_release_edit(const std::string&,const ReleaseEditIntent&,uint64_t,std::string&);
    bool submit_intervention(const std::string&,const InterventionIntent&,uint64_t,std::string&);
    bool submit_game_rules(const std::string&,const GameRulesIntent&,uint64_t,std::string&);
    bool submit_gm_policy(const std::string&,const GmPolicyIntent&,uint64_t,std::string&);
    bool submit_release_cycle(const std::string&,const ReleaseCycleIntent&,uint64_t,std::string&);
    bool submit_general_project(const std::string&,const GeneralProjectIntent&,uint64_t,std::string&);
    bool submit_logo(const std::string&,const LogoIntent&,uint64_t,std::string&);
    bool submit_network_remove(const std::string&,const std::string&,const NetworkRemoveIntent&,uint64_t,std::string&);
    bool submit_management(const std::string&,const ManagementIntent&,uint64_t,std::string&);
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
    bool submit_building_status(const std::string&,const BuildingStatusIntent&,uint64_t,std::string&);
    bool submit_dungeon_light(const std::string&,const DungeonLightIntent&,uint64_t,std::string&);
    bool submit_dungeon_gizmo(const std::string&,const DungeonGizmoIntent&,uint64_t,std::string&);
    bool submit_dungeon_palette(const std::string&,const DungeonPaletteIntent&,uint64_t,std::string&);
    bool submit_dungeon_sound(const std::string&,const DungeonSoundIntent&,uint64_t,std::string&);
    bool submit_dungeon_config(const std::string&,const DungeonConfigIntent&,uint64_t,std::string&);
    bool submit_costume_edit(const std::string&,const CostumeEditIntent&,uint64_t,std::string&);
    bool submit_ability_membership(const std::string&,const AbilityMembershipIntent&,uint64_t,std::string&);
    bool submit_ability_edit(const std::string&,const AbilityEditIntent&,uint64_t,std::string&);
    bool submit_character_membership(const std::string&,const CharacterMembershipIntent&,uint64_t,std::string&);
    bool submit_character_rename(const std::string&,const CharacterRenameIntent&,uint64_t,std::string&);
    bool submit_npc_quest_list(const std::string&,const std::string&,const NpcQuestListIntent&,uint64_t,std::string&);
    bool submit_npc_quest(const std::string&,const std::string&,const NpcQuestIntent&,uint64_t,std::string&);
    bool submit_remove_monster(const std::string&,const std::string&,const MonsterRemoveIntent&,uint64_t,std::string&);
    bool submit_move_monster(const std::string&,const std::string&,const MonsterMoveIntent&,uint64_t,std::string&);
    bool submit_monster(const std::string&,const MonsterIntent&,uint64_t,std::string&);
    bool submit_zone(const std::string& action_uuid,const ZoneIntent&,uint64_t now_ms,std::string& cause);
    bool submit_group(const std::string&,const GroupIntent&,uint64_t,std::string&);
    bool submit_boss_placement(const std::string&,const BossPlacementIntent&,uint64_t,std::string&);
    bool submit_world_path(const std::string&,const WorldPathIntent&,uint64_t,std::string&);
    bool submit_group_object(bool,const std::string&,const GroupObjectIntent&,uint64_t,std::string&);
    bool submit_scenery_stroke(const std::string&,const SceneryStrokeIntent&,uint64_t,std::string&);
    bool submit_duplicate(const std::string&,const DuplicateIntent&,uint64_t,std::string&);
    bool submit_district_title(const std::string&,const std::string&,const DistrictTitleIntent&,uint64_t,std::string&);
    bool submit_district_text(const std::string&,const std::string&,const DistrictTextIntent&,uint64_t,std::string&);
    bool submit_selection_move(const std::string&,const SelectionMoveIntent&,uint64_t,std::string&);
    bool submit_service_membership(const std::string&,const std::string&,const ServiceMembershipIntent&,uint64_t,std::string&);
    bool submit_boss_phase(const std::string&,const BossPhaseIntent&,uint64_t,std::string&);
    bool submit_prop_text(const std::string&,const std::string&,const PropTextIntent&,uint64_t,std::string&);
    bool submit_npc_property(const std::string&,const std::string&,const NpcPropertyIntent&,uint64_t,std::string&);
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
    Cached& remember(const std::string&,const Message&);
};
}
