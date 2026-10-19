#pragma once
#include "typed_action/general_project_action.h"
#include "typed_action/release_edit_action.h"
#include "typed_action/release_cycle_action.h"
#include "typed_action/gm_policy_action.h"
#include "typed_action/game_rules_action.h"
#include "typed_action/intervention_action.h"
#include "vendor/nlohmann/json.hpp"
#include <string>
#include <vector>
#include <cstdint>
namespace mt2mp::action_session::ui_capture {struct Replay;}
namespace mt2mp::action_session {struct SceneryStrokeIntent;}
namespace mt2mp::action_session {struct GroupObjectIntent;struct BossPlacementIntent;struct WorldPathIntent;}
namespace mt2mp::action_session {struct BuildingState;struct PriceIntent;struct ManagementIntent;struct NetworkRemoveIntent;struct LogoIntent;struct BugFixIntent;struct ZoneIntent;struct MonsterIntent;struct MonsterMoveIntent;struct MonsterRemoveIntent;struct NpcIntent;struct NpcPropertyIntent;struct PropTextIntent;struct BossPhaseIntent;struct ServiceMembershipIntent;struct SelectionMoveIntent;struct DistrictTextIntent;struct DistrictTitleIntent;struct GroupIntent;struct DuplicateIntent;struct NpcQuestIntent;struct NpcQuestListIntent;struct CharacterRenameIntent;struct CharacterMembershipIntent;struct Request;struct AbilityEditIntent;struct AbilityMembershipIntent;struct CostumeEditIntent;struct DungeonConfigIntent;struct DungeonSoundIntent;struct DungeonPaletteIntent;struct DungeonGizmoIntent;struct DungeonLightIntent;struct BuildingStatusIntent;struct Binding;struct Context;enum class Action:uint8_t;}

namespace mt2mp::game::developer_staff::construction {struct LocalJob;}
namespace mt2mp::game::typed_action_runtime {
bool start(std::string&);
bool read_subscriber_weapon(int32_t serialized_id,nlohmann::json& out,std::string& error);
void tick();
bool enable_live_state(std::string&);
bool live_state_active();
bool read_character_membership_actors(const action_session::Binding&,const action_session::Request&,
    nlohmann::json& economy,nlohmann::json& monsters,std::string&);
bool live_state_available(std::string&);
bool request_remove(const nlohmann::json&,nlohmann::json&,std::string&);
bool submit_place(const std::string& action_uuid,const action_session::BuildingState&,std::string&);
bool submit_move(const std::string& action_uuid,const std::string& object_key,const action_session::BuildingState&,std::string&);
bool submit_building_history(action_session::Action action,const std::string& action_uuid,const std::string& target_action_uuid,std::string&);
bool submit_building_history(action_session::Action action,const std::string& action_uuid,const std::string& target_action_uuid,const std::string& object_key,std::string&);
bool submit_start_research(const std::string& action_uuid,const std::string& key,std::string&);
bool submit_research_workers(const std::string& action_uuid,const std::string& key,int32_t desired,std::string&);
bool submit_commit_research(const std::string& action_uuid,const std::string& key,std::string&);
bool submit_commit_startingpoint(const std::string& action_uuid,std::string&);
bool submit_developer(bool hire,const std::string& id,std::string& error);
bool submit_bug_fix(const std::string&,const action_session::BugFixIntent&,std::string&);
bool submit_startingpoint_workers(const std::string& id,int32_t desired,std::string& error);
bool submit_start_startingpoint(const std::string& action_uuid,std::string&);
bool read_intervention_live(const action_session::Context&,const action_session::Binding&,
                            nlohmann::json& economy,nlohmann::json& monsters,nlohmann::json& marketing,std::string&);
bool submit_intervention(const std::string&,const action_session::InterventionIntent&,std::string&);
bool submit_game_rules(const std::string&,const action_session::GameRulesIntent&,std::string&);
bool submit_gm_policy(const std::string&,const action_session::GmPolicyIntent&,std::string&);
bool submit_release_cycle(const std::string&,const action_session::ReleaseCycleIntent&,std::string&);
bool submit_release_edit(const std::string&,const action_session::ReleaseEditIntent&,std::string&);
bool submit_general_project(const std::string&,const action_session::GeneralProjectIntent&,std::string&);
bool submit_logo(const std::string&,const action_session::LogoIntent&,std::string&);
bool submit_network_remove(const std::string&,const std::string&,const action_session::NetworkRemoveIntent&,std::string&);
bool submit_management(const std::string&,const action_session::ManagementIntent&,std::string&);
bool submit_prices(const std::string& action_uuid,const action_session::PriceIntent&,std::string&);
bool submit_building_status(const std::string&,const action_session::BuildingStatusIntent&,std::string&);
bool submit_dungeon_light(const std::string&,const action_session::DungeonLightIntent&,std::string&);
bool submit_dungeon_gizmo(const std::string&,const action_session::DungeonGizmoIntent&,std::string&);
bool submit_dungeon_palette(const std::string&,const action_session::DungeonPaletteIntent&,std::string&);
bool submit_dungeon_sound(const std::string&,const action_session::DungeonSoundIntent&,std::string&);
bool submit_dungeon_config(const std::string&,const action_session::DungeonConfigIntent&,std::string&);
bool submit_costume_edit(const std::string&,const action_session::CostumeEditIntent&,std::string&);
bool submit_ability_membership(const std::string&,const action_session::AbilityMembershipIntent&,std::string&);
bool submit_ability_edit(const std::string&,const action_session::AbilityEditIntent&,std::string&);
bool submit_character_membership(const std::string&,const action_session::CharacterMembershipIntent&,std::string&);
bool submit_character_rename(const std::string&,const action_session::CharacterRenameIntent&,std::string&);
bool submit_npc_quest_list(const std::string&,const std::string&,const action_session::NpcQuestListIntent&,std::string&);
bool submit_npc_quest(const std::string&,const std::string&,const action_session::NpcQuestIntent&,std::string&);
bool submit_remove_monster(const std::string&,const std::string&,const action_session::MonsterRemoveIntent&,std::string&);
bool submit_move_monster(const std::string&,const std::string&,const action_session::MonsterMoveIntent&,std::string&);
bool submit_monster(const std::string&,const action_session::MonsterIntent&,std::string&);
bool submit_zone(const std::string&,const action_session::ZoneIntent&,std::string&);
bool submit_group(const std::string&,const action_session::GroupIntent&,std::string&);
bool submit_boss_placement(const std::string&,const action_session::BossPlacementIntent&,std::string&);
bool submit_world_path(const std::string&,const action_session::WorldPathIntent&,std::string&);
bool submit_group_object(bool,const std::string&,const action_session::GroupObjectIntent&,std::string&);
bool submit_duplicate(const std::string&,const action_session::DuplicateIntent&,std::string&);
bool submit_scenery_stroke(const std::string&,const action_session::SceneryStrokeIntent&,std::string&);
bool submit_district_title(const std::string&,const std::string&,const action_session::DistrictTitleIntent&,std::string&);
bool submit_district_text(const std::string&,const std::string&,const action_session::DistrictTextIntent&,std::string&);
bool submit_selection_move(const std::string&,const action_session::SelectionMoveIntent&,std::string&);
bool submit_service_membership(const std::string&,const std::string&,const action_session::ServiceMembershipIntent&,std::string&);
bool submit_boss_phase(const std::string&,const action_session::BossPhaseIntent&,std::string&);
bool submit_prop_text(const std::string&,const std::string&,const action_session::PropTextIntent&,std::string&);
bool submit_npc_property(const std::string&,const std::string&,const action_session::NpcPropertyIntent&,std::string&);
bool submit_npc(const std::string& action_uuid,const action_session::NpcIntent&,std::string&);
bool submit_remove_npc(const std::string& action_uuid,const std::string& target_action_uuid,std::string&);
bool submit_move_npc(const std::string& action_uuid,const std::string& target_action_uuid,
    const action_session::BuildingState& transform,std::string&);
bool observe_startingpoint_submission(const action_session::Context& expected_context,
    const action_session::Binding& expected_binding,const std::string& action_uuid,std::string&);
bool bug_parent_alive(const action_session::Context&,const action_session::Binding&,const std::string& key,
    bool& known,bool& alive,std::string&);
bool bug_context(action_session::Context&,action_session::Binding&,uint64_t& publication,bool& active,std::string&);
// Query the existing Controller binding during capture/apply; this does not admit a new action.
bool read_binding(const std::string& snapshot,action_session::Binding&,std::string&);
// New native UI intents require the idle admission gate.
bool network_binding(const std::string& snapshot,action_session::Binding&,std::string&);
bool construction_jobs(const action_session::Context&,const action_session::Binding&,
    std::vector<developer_staff::construction::LocalJob>&,std::string&);
bool request_place(const nlohmann::json&,nlohmann::json&,std::string&);
bool request_move(const nlohmann::json&,nlohmann::json&,std::string&);
nlohmann::json controller_status();
nlohmann::json status();
bool authorize_ui_replay(const action_session::ui_capture::Replay&,std::string&);
}

namespace mt2mp::game::typed_action_runtime { bool begin_lobby_leave(std::string&); bool finish_lobby_leave(bool& ready,std::string&); }

namespace mt2mp::game::typed_action_runtime {
bool begin_world_reload(std::string& error);
bool finish_world_reload(bool& ready,std::string& error);
}
