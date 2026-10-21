#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include "typed_action/management_action.h"
#include "service_live_state.h"
#include "analytics_state.h"
#include "subscriber_demand_state.h"
#include "scenario_intro_state.h"
#include "toon_thought_state.h"
#include "dungeon_gizmo_live_state.h"
#include "general_project_codec.h"
#include "gm_ticket_publication.h"
#include "gm_policy_state.h"
#include "game_rules_state.h"
#include "subscriber_intervention.h"
#include "intervention_actor_state.h"
#include "intervention_actor_lifecycle.h"
#include <optional>

namespace mt2mp::game::management_simulation {
struct LiveState {
    service_live::State services;general_project::Progress project;
    std::optional<analytics_state::State> analytics;
    std::optional<scenario_intro::State> intro;
    std::optional<toon_thought::State> thoughts;
    std::optional<dungeon_gizmo_live::State> gizmos;
    std::optional<character_calendar::State> character_calendar;
    std::optional<subscriber_demand::State> subscriber_demands;
    std::optional<gm_ticket::Publication> gm;
    std::optional<gm_policy::State> policies;
    std::optional<game_rules::State> rules;
    std::optional<subscriber_intervention::State> subscriber_interventions;
    std::optional<interventions::actors::State> intervention_actors;
    std::optional<interventions::actors::lifecycle::Batch> actor_lifecycle;
    int32_t game_masters=0,release_funds=0,free_points=0,purchased_points=0;
    std::array<int32_t,10> statistics{},spent{};
    std::array<uint8_t,10> purchased{};
    std::array<int32_t,3> character_slots{},software_version{},planned_version{};
    bool release_present=false;int32_t release_type=0;
    int32_t lounge_employed=0;std::vector<std::array<uint8_t,2>> gm_employment;
    bool operator==(const LiveState&)const=default;
};
inline LiveState project(const action_session::ManagementState& v){LiveState out;out.game_masters=v.game_masters;out.release_funds=v.release_funds;out.free_points=v.statistics_points;out.purchased_points=v.purchased_points;out.statistics=v.statistics;out.spent=v.spent;out.purchased=v.purchased;out.character_slots=v.character_slots;out.software_version=v.software_version;out.planned_version=v.planned_version;out.release_present=v.release_present;out.release_type=v.release_type;out.lounge_employed=v.lounge_employed;out.gm_employment=v.gm_employment;return out;}
inline bool valid(const LiveState& v,std::string& e){
    if(!service_live::valid(v.services,e))return false;
    if(v.analytics&&!analytics_state::valid(*v.analytics,e))return false;
    if(v.intro&&!scenario_intro::valid(*v.intro,e))return false;
    if(v.thoughts&&!toon_thought::valid(*v.thoughts,e))return false;
    if(v.gizmos&&!dungeon_gizmo_live::valid(*v.gizmos,e))return false;
    if(v.character_calendar&&!character_calendar::valid(*v.character_calendar,e))return false;
    if(v.subscriber_demands&&(!v.character_calendar||!subscriber_demand::reciprocal(*v.subscriber_demands,*v.character_calendar,e))){if(e.empty())e="Subscriber Demand references require complete Character calendar";return false;}
    if(!general_project::valid(v.project)){e="Management Frame current project progress invalid";return false;}
    if(v.game_masters<0||v.game_masters>50||v.release_funds<0||v.free_points<0||v.purchased_points<0||v.release_type<0||v.release_type>4||(!v.release_present&&(v.release_type||v.planned_version!=std::array<int32_t,3>{}))){e="Management live scalar domain invalid";return false;}
    for(const auto* values:{&v.statistics,&v.spent})for(auto n:*values)if(n<0){e="Management live statistics negative";return false;}
    for(const auto* values:{&v.character_slots,&v.software_version,&v.planned_version})for(auto n:*values)if(n<0){e="Management live slots/version negative";return false;}
    if(v.lounge_employed<0||v.lounge_employed>50||v.gm_employment.size()>50){e="Management native Lounge extent invalid";return false;}
    if(!v.gm_employment.empty()&&v.gm_employment.size()!=50){e="Management native Lounge generated pool must have50 fixed slots";return false;}
    for(size_t n=0;n<v.gm_employment.size();++n)if(v.gm_employment[n][0]!=uint8_t(n<size_t(v.lounge_employed))||v.gm_employment[n][1]>1){e="Management native Lounge employment/return flags invalid";return false;}
    if(v.actor_lifecycle&&(!v.intervention_actors||!interventions::actors::lifecycle::valid(*v.actor_lifecycle,e))){if(e.empty())e="Actor lifecycle requires complete current Actor values";return false;}
    if(v.intervention_actors&&(!v.subscriber_interventions||!interventions::actors::valid(*v.intervention_actors,e))){if(e.empty())e="Actor values require complete Subscriber intervention values";return false;}
    if(v.subscriber_interventions&&(!v.rules||!subscriber_intervention::valid(*v.subscriber_interventions,e))){if(e.empty())e="Subscriber intervention values require complete Management rules/GM";return false;}
    if(v.rules&&(!v.policies||!v.gm||!game_rules::valid(*v.rules))){e="Management Design rules require complete current policy/GM values";return false;}
    if(v.policies&&(!v.gm||!gm_policy::valid(*v.policies))){e="Management policy state requires complete GM publication";return false;}
    if(v.gm){if(!gm_ticket::valid(*v.gm,e))return false;
        if(v.gm->state.masters.size()!=v.gm_employment.size()){e="Management GM publication fixed pool extent differs";return false;}
        for(size_t n=0;n<v.gm_employment.size();++n)if(v.gm->state.masters[n].employed!=bool(v.gm_employment[n][0])||v.gm->state.masters[n].active!=bool(v.gm_employment[n][1])){e="Management GM publication employment/active flags differ";return false;}}
    for(auto n:v.purchased)if(n>1){e="Management live purchase flag invalid";return false;}e.clear();return true;
}
inline nlohmann::json encode(const LiveState& v){nlohmann::json out={{"game_masters",v.game_masters},{"release_funds",v.release_funds},{"free_points",v.free_points},{"purchased_points",v.purchased_points},
    {"statistics",v.statistics},{"spent",v.spent},{"purchased",v.purchased},{"character_slots",v.character_slots},{"software_version",v.software_version},{"planned_version",v.planned_version},{"release_present",v.release_present},{"release_type",v.release_type},{"lounge_employed",v.lounge_employed},{"gm_employment",v.gm_employment},{"services",service_live::encode(v.services)},{"project",general_project::encode(v.project)}};if(v.analytics)out["analytics"]=analytics_state::encode(*v.analytics);if(v.intro)out["intro"]=scenario_intro::encode(*v.intro);if(v.thoughts)out["thoughts"]=toon_thought::encode(*v.thoughts);if(v.gizmos)out["gizmos"]=dungeon_gizmo_live::encode(*v.gizmos);if(v.character_calendar)out["character_calendar"]=character_calendar::encode(*v.character_calendar);if(v.subscriber_demands)out["subscriber_demands"]=subscriber_demand::encode(*v.subscriber_demands);if(v.gm)out["gm"]=gm_ticket::encode(*v.gm);if(v.policies)out["policies"]=gm_policy::encode(*v.policies);if(v.rules)out["rules"]=game_rules::encode(*v.rules);if(v.subscriber_interventions)out["subscriber_interventions"]=subscriber_intervention::encode(*v.subscriber_interventions);if(v.intervention_actors)out["intervention_actors"]=interventions::actors::encode(*v.intervention_actors);if(v.actor_lifecycle)out["actor_lifecycle"]=interventions::actors::lifecycle::encode(*v.actor_lifecycle);return out;}
inline bool decode(const nlohmann::json& j,LiveState& out,std::string& e){
    try{LiveState v;const auto number=[](const nlohmann::json& n){if(!n.is_number_integer()||(n.is_number_unsigned()?n.get<uint64_t>()>uint64_t(INT32_MAX):n.get<int64_t>()<0||n.get<int64_t>()>INT32_MAX))throw std::runtime_error("Management live int32 outside domain");return n.get<int32_t>();};
        if(!j.is_object()||(j.size()!=16+size_t(j.contains("gizmos"))+size_t(j.contains("thoughts"))+size_t(j.contains("intro"))+size_t(j.contains("character_calendar"))+size_t(j.contains("subscriber_demands"))+size_t(j.contains("analytics"))+size_t(j.contains("gm"))+size_t(j.contains("policies"))+size_t(j.contains("rules"))+size_t(j.contains("subscriber_interventions"))+size_t(j.contains("intervention_actors"))+size_t(j.contains("actor_lifecycle"))))throw std::runtime_error("Management live exact base fields with optional GM and policy values required");
        v.game_masters=number(j.at("game_masters"));v.release_funds=number(j.at("release_funds"));v.free_points=number(j.at("free_points"));v.purchased_points=number(j.at("purchased_points"));v.release_type=number(j.at("release_type"));
        if(!j.at("release_present").is_boolean())throw std::runtime_error("Management live release presence requires bool");v.release_present=j.at("release_present").get<bool>();
        auto array=[&](const char* name,auto& target){const auto& a=j.at(name);if(!a.is_array()||a.size()!=target.size())throw std::runtime_error("Management live array extent invalid");for(size_t n=0;n<target.size();++n){auto value=number(a[n]);if constexpr(sizeof(target[n])==1){if(value>1)throw std::runtime_error("Management live flag outside0/1");}target[n]=typename std::remove_reference_t<decltype(target)>::value_type(value);}};
        array("statistics",v.statistics);array("spent",v.spent);array("purchased",v.purchased);array("character_slots",v.character_slots);array("software_version",v.software_version);array("planned_version",v.planned_version);
        v.lounge_employed=number(j.at("lounge_employed"));const auto& pool=j.at("gm_employment");if(!pool.is_array()||pool.size()>50)throw std::runtime_error("Management native Lounge rows invalid");
        for(const auto& row:pool){if(!row.is_array()||row.size()!=2)throw std::runtime_error("Management employment pair required");const auto employed=number(row[0]),returning=number(row[1]);if(employed>1||returning>1)throw std::runtime_error("Management employment Boolean required");v.gm_employment.push_back({uint8_t(employed),uint8_t(returning)});}
        v.project=general_project::codec::progress(j.at("project"));
        if(j.contains("character_calendar")){character_calendar::State value;if(!character_calendar::decode(j.at("character_calendar"),value,e))return false;v.character_calendar=std::move(value);}
        if(j.contains("subscriber_demands")){subscriber_demand::State value;if(!subscriber_demand::decode(j.at("subscriber_demands"),value,e))return false;v.subscriber_demands=std::move(value);}
        if(j.contains("gizmos")){dungeon_gizmo_live::State value;if(!dungeon_gizmo_live::decode(j.at("gizmos"),value,e))return false;v.gizmos=std::move(value);}
        if(j.contains("thoughts")){toon_thought::State value;if(!toon_thought::decode(j.at("thoughts"),value,e))return false;v.thoughts=std::move(value);}
        if(j.contains("intro")){scenario_intro::State value;if(!scenario_intro::decode(j.at("intro"),value,e))return false;v.intro=value;}
        if(j.contains("analytics")){analytics_state::State value;if(!analytics_state::decode(j.at("analytics"),value,e))return false;v.analytics=std::move(value);}
        if(j.contains("actor_lifecycle")){interventions::actors::lifecycle::Batch batch;if(!interventions::actors::lifecycle::decode(j.at("actor_lifecycle"),batch,e))return false;v.actor_lifecycle=std::move(batch);}
        if(j.contains("intervention_actors"))v.intervention_actors=interventions::actors::codec::state(j.at("intervention_actors"));
        if(j.contains("subscriber_interventions")){subscriber_intervention::State state;if(!subscriber_intervention::decode(j.at("subscriber_interventions"),state,e))return false;v.subscriber_interventions=std::move(state);}
        if(j.contains("rules"))v.rules=game_rules::codec::state(j.at("rules"));
        if(j.contains("policies"))v.policies=gm_policy::codec::state(j.at("policies"));
        if(j.contains("gm")){gm_ticket::Publication value;if(!gm_ticket::decode(j.at("gm"),value,e))return false;v.gm=std::move(value);}
        if(!service_live::decode(j.at("services"),v.services,e)||!valid(v,e))return false;out=v;e.clear();return true;
    }catch(const std::exception& x){e=std::string("Management live decode: ")+x.what();return false;}
}
struct Change {size_t slot=0;int32_t before=0,after=0;bool operator==(const Change&)const=default;};
inline std::array<int32_t,40> scalars(const LiveState& v){
    std::array<int32_t,40> out{};size_t n=0;for(auto value:{v.game_masters,v.release_funds,v.free_points,v.purchased_points})out[n++]=value;
    for(auto value:v.statistics)out[n++]=value;for(auto value:v.spent)out[n++]=value;for(auto value:v.purchased)out[n++]=value;
    for(auto value:v.software_version)out[n++]=value;for(auto value:v.planned_version)out[n++]=value;return out;
}
inline bool plan(const LiveState& before,const LiveState& target,std::vector<Change>& out,std::string& e){
    out.clear();if(!valid(before,e)||!valid(target,e)||!service_live::compatible(before.services,target.services,e))return false;
    if(before.project.current!=target.project.current){e="Management Frame current project membership requires its ordered original transaction";return false;}
    if(before.release_present!=target.release_present){e="Management live Release membership requires its ordered original transaction";return false;}
    if(before.gm_employment.size()!=target.gm_employment.size()){e="Management live Lounge pool creation requires native fixed-slot identity reconciliation";return false;}
    auto a=scalars(before),b=scalars(target);for(size_t n=0;n<a.size();++n)if(a[n]!=b[n])out.push_back({n,a[n],b[n]});
    if(before.release_type!=target.release_type)out.push_back({40,before.release_type,target.release_type});
    for(size_t n=0;n<3;++n)if(before.character_slots[n]!=target.character_slots[n])out.push_back({41+n,before.character_slots[n],target.character_slots[n]});
    if(before.lounge_employed!=target.lounge_employed)out.push_back({44,before.lounge_employed,target.lounge_employed});
    for(size_t n=0;n<before.gm_employment.size();++n)for(size_t field=0;field<2;++field)if(before.gm_employment[n][field]!=target.gm_employment[n][field])out.push_back({45+n*2+field,before.gm_employment[n][field],target.gm_employment[n][field]});e.clear();return true;
}
}
