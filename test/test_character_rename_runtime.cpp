#include "typed_action/action_session.h"
#include "typed_action/ordered_building_family.h"
#include "release_edit_state.h"
namespace release_edit=mt2mp::game::release_edit;
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;
namespace a=mt2mp::action_session;
bool fail(std::string& error,const std::string& why){error=why;return false;}
unsigned checks=0,original_count=0,seed_count=0,submit_count=0,begin_rename_count=0,drop_focus_count=0,selection_rebuilds=0;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
const std::string fixture_snapshot="22222222-2222-4222-8222-222222222222";
const a::Binding expected_binding{2,3};
a::Request fixture_request;
a::Context session;
bool scoped=true,scope_valid=true,original_go=false,binding_valid=true,session_valid=true;
bool preparation_rng=false,preparation_effect=false,original_returned=true,cleanup=true;
bool bad_effect=false,bad_ledger=false,bad_owner=false,read_failed=false;
json fixture_controller;
struct Config {bool is_host=true,local_test=true;static Config& instance(){static Config c;return c;}};
unsigned GetCurrentThreadId(){return 7;}
namespace net {
std::string current_snapshot_id(){return fixture_snapshot;}
namespace session_wire {bool connected(){return true;}}
}
namespace mt2mp::action_session {
struct FixtureSession {bool current(Context& out,std::string& e){if(!session_valid)return fail(e,"Session unavailable");out=session;return true;}};
bool network_history(const Request&){return false;}
FixtureSession& selected_session(){static FixtureSession s;return s;}
namespace tick_commands::authority {
bool active(){return scoped;}
bool matches(const Context& c,const Binding& b,const Request& r,std::string& e){return
    (scoped&&scope_valid&&c==session&&b==expected_binding&&r==fixture_request)||fail(e,"Consumed command differs");}
bool matches(const Binding& b,const Request& r,std::string& e){return matches(session,b,r,e);}
}
namespace ui_diagnostic {
using UiCommand=bool(*)(void*,void*);
struct Api {uintptr_t label_equals=0;UiCommand command=nullptr;const char* destroy_label=nullptr;};
struct Window {uintptr_t window=0;};
struct Receipt {bool ui_command_returned=false,record_cleanup_confirmed=false;};
}
}
struct Memory {};
namespace typed_action_runtime {
json controller_status(){return fixture_controller;}
bool original_go_admission(bool& inside,std::string&){inside=original_go;return true;}
bool read_binding(const std::string& s,a::Binding& b,std::string& e){if(!binding_valid||s!=fixture_snapshot)return fail(e,"Binding differs");b=expected_binding;return true;}
bool network_binding(const std::string& s,a::Binding& b,std::string& e){if(original_go)return fail(e,"Network binding forbidden in OriginalGo");return read_binding(s,b,e);}
bool submit_character_rename(const std::string&,const a::CharacterRenameIntent&,std::string&){++submit_count;return true;}
}
struct Sample {
    unsigned owner=1;unsigned world=1;uint64_t uid=77;unsigned undo=0;a::CharacterRenameState value;a::Ledger money;json proof=json::object();
    uintptr_t selected=0x400,manager=0x500;
};
struct Pending {a::Binding binding;std::string snapshot;a::CharacterRenameIntent intent;std::optional<a::Context> context;std::string diagnostic_token;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false,seed_attempted=false,ordered=false,ui_restored=false;uint32_t calls=0,presentation_calls=0,preparation_notifications=0;std::string cause;json ui_state=json::object();};
struct State {unsigned thread=7;uintptr_t image=0x1000;} state;
bool ready=true,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint8_t replay_property=0;uint32_t original_calls=0;
uint64_t captures=0,submitted=0,rejected=0;std::string cause,last_action;
std::optional<Pending> pending;std::map<std::string,Operation> operations;
Sample live;
bool entry(void*,void*);
bool presentation(void*,void*,uintptr_t,bool&,std::string&);
uint32_t set_view_text(uintptr_t,const char*,size_t);
std::string active_ui_token;
bool diagnostic_pending();bool diagnostic_submitted(const std::string&,const std::string&,std::string&);void diagnostic_retire_lobby();
#include "character_rename_view_types_actual.inc"
uintptr_t ui_selected=0x400,ui_focus=0;std::string ui_text="New name",ui_input_cache="Input cache",ui_commit_baseline="Commit baseline";
std::array<uint32_t,2> ui_numeric_cache{3,4};std::array<uint32_t,8> ui_selection{};
uint8_t ui_disabled=1,ui_editing=0,ui_selectable=0;
bool route_valid=true,bound_button=false,restore_failed=false,drop_focus_noop=false;

using Command=a::ui_diagnostic::UiCommand;
struct FixtureRecord {std::string label;std::vector<std::string> arguments;};
std::map<const void*,const FixtureRecord*> fixture_records;
std::string fixture_record_label(const void* record){const auto found=fixture_records.find(record);
    return found==fixture_records.end()?std::string(static_cast<const char*>(record)):found->second->label;}
bool dispatch_fixture_record(const FixtureRecord& record){fixture_records.emplace(&record,&record);
    const bool result=entry(reinterpret_cast<void*>(0x100),const_cast<FixtureRecord*>(&record));fixture_records.erase(&record);return result;}
bool native_original(void*,void* record){
    const auto label=fixture_record_label(record);
    if(label=="BeginRename"){++begin_rename_count;ui_focus=0x200;ui_editing=1;ui_commit_baseline=ui_input_cache=ui_text;
        if(preparation_rng)++live.value.global_random[0];
        set_view_text(0x200,live.value.name.c_str(),live.value.name.size());return true;}
    if(label=="DoRename"&&ui_text==live.value.name)return true;
    ++original_count;if(!original_returned)return false;
    live.value.name=live.value.definition_name=fixture_request.character_rename.name;
    live.value.rename_day=live.value.day;live.value.edited=true;++live.value.release_edits[7];
    live.value.all_sha256=std::string(64,'c');++live.value.global_random[0];
    if(bad_effect)live.value.others_sha256=std::string(64,'d');
    if(bad_ledger)++live.money.cash;if(bad_owner)++live.owner;ui_disabled=0;
    bool handled=false;std::string error;if(!presentation(reinterpret_cast<void*>(0x100),reinterpret_cast<void*>(0x400),state.image+0x68a694,handled,error))return false;return true;
}
Command original=&native_original;
bool reject(const std::string& e){++rejected;cause=e;return false;}
namespace character_membership {bool capture_variant(void*,void*,bool& handled,std::string&){handled=false;return true;}}
namespace ability_edit {
bool presentation_route(std::string& error){return route_valid||fail(error,"Presentation route differs");}
bool capture_adjustment(void*,void*,bool& handled,std::string&){handled=false;return true;}
bool pending_ui(){return false;}
}
namespace costume_edit {bool pending_ui(){return false;}}
namespace dungeon_config {bool pending_ui(){return false;}}
namespace dungeon_sound {bool pending_ui(){return false;}}
namespace dungeon_gizmo {bool pending_ui(){return false;}}
namespace dungeon_palette {bool pending_ui(){return false;}}
namespace dungeon_light {bool pending_ui(){return false;}}
namespace building_status {bool pending_ui(){return false;}}
namespace quest_list {
struct QuestTitleView {const char* data;uint64_t size,capacity=0,padding=0;};
bool pending_ui(){return false;}
uint32_t label(uintptr_t,void* record,const char* text,bool& result){const auto found=fixture_records.find(record);
    result=fixture_record_label(record)==text&&(found==fixture_records.end()||found->second->arguments.empty());return 0;}
}
#include "character_rename_command_label_actual.inc"
bool fixture_token_equals(const void* record,const quest_list::QuestTitleView& text){return fixture_record_label(record)==std::string(text.data,text.size);}
uint32_t native_command_label(void* record,const char* wanted,bool& result){result=command_label_matches(&fixture_token_equals,record,wanted);return 0;}
namespace action_runtime {
std::string uuid(std::string&){return fixture_request.action_uuid;}
uint32_t seed(uintptr_t,const std::array<uint64_t,4>&){++seed_count;return 0;}
}
bool code(Memory&,std::string&){return true;}
std::map<std::pair<uintptr_t,size_t>,uint64_t> native_values;
std::map<std::pair<uintptr_t,size_t>,std::string> native_text;
template<class T>bool field(Memory&,uintptr_t p,size_t offset,T& out,std::string& e){
    if constexpr(std::is_same_v<T,std::array<uint32_t,2>>){if(p==0x200&&offset==0x4d0){out=ui_numeric_cache;return true;}}
    else if constexpr(std::is_same_v<T,std::array<uint32_t,8>>){if(p==0x200&&offset==0x544){out=ui_selection;return true;}}
    else if constexpr(std::is_integral_v<T>){
        const auto supplied=native_values.find({p,offset});if(supplied!=native_values.end()){out=T(supplied->second);return true;}
        if constexpr(sizeof(T)==8){
            if(p==0x100&&offset==0x460){out=T(ui_selected);return true;}
            if(p==0x100&&offset==0x360){out=T(0x200);return true;}
            if(p==0x100&&offset==0x280){out=T(0x300);return true;}
            if(p==0x100&&offset==0x278){out=T(0x300);return true;}
            if(p==0x100&&offset==0x370){out=T(0x600);return true;}
            if(p==0x100&&offset==0x358){out=T(0x700);return true;}
            if(p==0x100&&offset==0){out=T(state.image+0x1736770);return true;}
            if(p==state.image+0x1736770&&offset==0x178){out=T(state.image+0x8167c0);return true;}
            if(p==state.image+0x16e3ec0&&offset==0x178){out=T(state.image+0xc35730);return true;}
            if(p==0x100&&offset==0x2d0){out=T(0x1800);return true;}
            if(p==0x1800&&offset==0){out=T(0x600);return true;}
            if(p==0x100&&offset==0x300){out=0;return true;}
            if(p==0x700&&offset==0){out=T(state.image+0x16dfe90);return true;}
            if(p==0x700&&offset==0x388){out=T(0x780);return true;}
            if(p==0x780&&offset==0){out=T(0x200);return true;}
            if(p==0x700&&(offset==0x410||offset==0x418)){out=T(0x200);return true;}
            if(p==0x200&&offset==0){out=T(state.image+0x1703290);return true;}
            if(p==0x600&&offset==0){out=T(state.image+0x16e3ec0);return true;}
            if((p==0x200||p==0x600||p==0x700)&&offset==0x280){out=T(0x300);return true;}
            if((p==0x600||p==0x700)&&offset==0x278){out=T(0x100);return true;}
            if(p==0x200&&offset==0x278){out=T(0x700);return true;}
            if(p==0x600&&offset==0x578){out=T(bound_button?0x777:0);return true;}
            if(p==0x200&&offset==0x4a0){out=0;return true;}
            if(p==0x300&&offset==0x3c0){out=T(ui_focus);return true;}
        }
        if constexpr(sizeof(T)==4){if(p==0x400&&(offset==0x2c||offset==0x30)){out=T(offset==0x2c?0:1);return true;}
            if(p==0x700&&(offset==0x390||offset==0x394)){out=1;return true;}
            if(p==0x100&&(offset==0x2d8||offset==0x2dc)){out=1;return true;}
            if(p==0x100&&(offset==0x308||offset==0x30c)){out=0;return true;}}
        if constexpr(sizeof(T)==1){
            if((p==0x200||p==0x600||p==0x700||p==0x100)&&offset==0x69){out=1;return true;}
            if(p==0x300&&offset==0x45f){out=1;return true;}
            if(p==0x600&&(offset==0x584||offset==0x585)){out=0;return true;}
            if(p==0x600&&offset==0x46d){out=T(ui_disabled);return true;}
            if(p==0x600&&offset==0x174){out=1;return true;}
            if(p==0x200&&offset==0x584){out=ui_editing;return true;}
            if(p==0x200&&offset==0x445){out=ui_selectable;return true;}
        }
    }
    return fail(e,"Unexpected field");
}
namespace npc_quest_records {bool read_string(Memory&,uintptr_t p,size_t offset,const char*,std::string& out,std::string& e){
    const auto supplied=native_text.find({p,offset});if(supplied!=native_text.end()){out=supplied->second;return true;}
    if(p==0x900&&offset==0x1b0){out="collection_classes";return true;}
    if(p==0x600&&offset==0x388){out="MessageTo Parent BeginRename";return true;}
    if((p==0x600||p==0x100)&&offset==0x1b0){out=p==0x100?"character_view":"";return true;}
    if(p==0x700&&offset==0x1b0){out="general_name_multiview";return true;}
    if(p==0x200&&offset==0x1b0){out="general_edit_name";return true;}
    if(p==0x200&&offset==0x3e0){out="MessageTo Parent DoRename";return true;}
    if(p==0x200&&offset==0x400){out.clear();return true;}
    if(p==0x200&&offset==0x4a8){out=ui_input_cache;return true;}
    if(p==0x200&&offset==0x4d8){out=ui_commit_baseline;return true;}
    if(p==0x200&&offset==0x2d0){out=ui_text;return true;}return fail(e,"Unexpected native string");
}}
bool window(Memory&,uintptr_t expected,uintptr_t& view,uintptr_t& editor,uintptr_t& outer,std::string& e){
    if(expected&&expected!=ui_selected)return fail(e,"Selected object differs");view=0x100;editor=0x200;outer=0x300;return true;
}
bool read_input(Memory&,uintptr_t,uintptr_t,a::CharacterRenameIntent& intent,std::string&){intent.name=ui_text;return true;}
bool sample(Memory&,const a::Binding&,const std::string&,const a::CharacterRenameIntent&,Sample& out,std::string& e){
    if(read_failed&&original_count)return fail(e,"Retained complete record read failure");out=live;return true;
}
bool same_local(const Sample& before,const Sample& after){return before.owner==after.owner&&before.money==after.money&&before.selected==after.selected&&before.manager==after.manager;}
uint32_t name_in_use(uintptr_t,const char*,size_t,bool& duplicate){duplicate=false;return 0;}
uint32_t set_window(uintptr_t,uintptr_t,uintptr_t,uintptr_t,const char*,size_t){
    if(preparation_rng)++live.value.global_random[0];if(preparation_effect)++live.value.release_edits[0];return 0;
}
bool prepare_stat(Memory&,uintptr_t,const a::CharacterRenameIntent&,std::string&){return true;}
uint32_t write_view_target(uintptr_t,uintptr_t selected){if(restore_failed&&original_count)return 1;ui_selected=selected;return 0;}
uint32_t write_view_numeric(uintptr_t,const std::array<uint32_t,2>& value){ui_numeric_cache=value;return 0;}
uint32_t restore_view_selection(uintptr_t,const std::array<uint32_t,8>& value){
    ++selection_rebuilds;ui_selection=value;return 0;
}
uint32_t drop_view_focus(uintptr_t editor){
    ++drop_focus_count;if(drop_focus_noop||ui_focus!=editor)return 0;ui_focus=0;ui_editing=0;
    ui_selection={0,0xffffffffu,0,0xffffffffu,0,0xffffffffu,0,0xffffffffu};
    if(ui_text!=ui_commit_baseline){ui_commit_baseline=ui_text;dispatch_fixture_record({"DoRename",{ui_text}});}return 0;
}
uint32_t set_view_text(uintptr_t,const char* text,size_t length){
    const std::string next(text,length);if(next==ui_text)return 0;ui_text=next;ui_numeric_cache={0,0};
    if(preparation_rng)++live.value.global_random[0];if(preparation_effect)++live.value.release_edits[0];
    return 0;
}
uint32_t set_view_disabled(uintptr_t,bool disabled){ui_disabled=disabled?1:0;return 0;}
uint32_t dirty_view_title(uintptr_t){return 0;}
#include "character_rename_view_actual.inc"
const char* stat_label(uint8_t p){return p?"SetHealth":"DoRename";}
struct UiCode {uintptr_t rva=0x689e80;} ui_code;
namespace mt2mp::action_session::ui_diagnostic {
bool resolve_api(Memory&,uintptr_t,const char*,Api&,std::string&){return true;}
struct Calls {bool command(const Api& api,const Window& window,Receipt& receipt,std::string&){
    const bool opening=std::string(api.destroy_label)=="clicked";
    const bool accepted=opening?(window.window==0xa00):entry(reinterpret_cast<void*>(0x100),const_cast<char*>(api.destroy_label));
    if(opening&&preparation_effect)++live.value.release_edits[0];
    receipt.ui_command_returned=original_returned;receipt.record_cleanup_confirmed=cleanup;return accepted;
}};
Calls& process_calls(){static Calls calls;return calls;}
}
const char* exe_sha="fixture";
namespace character_membership {
struct Sample {unsigned world=1;uintptr_t manager=0x500,selected=0x400;uint64_t uid=77;unsigned undo=0;a::Ledger money;a::CharacterMembershipState value;std::string other_users_sha;};
Sample complete;
bool read_only_requested=false;
bool sample(Memory&,const a::Binding&,const std::string&,const a::CharacterMembershipIntent&,Sample& out,std::string&,const Sample* previous,const a::Request* action,bool read_only){
    read_only_requested=read_only&&!previous&&!action;out=complete;return true;
}
}
bool same_world(unsigned x,unsigned y){return x==y;}
namespace quest_bridge {bool sha256(const std::string& value,std::string& out,std::string&){out=value;return true;}}
#include "character_rename_proof_actual.inc"
#include "character_rename_admission_actual.inc"
#include "character_rename_capture_actual.inc"
#include "character_rename_presentation_actual.inc"
#include "character_rename_tick_actual.inc"
#include "character_rename_execution_actual.inc"
namespace ui_move_runtime {
namespace selection_move {bool history_intent(const a::Binding&,const a::Request&,a::SelectionMoveIntent&,std::string& e){return fail(e,"Unexpected selection");}}
namespace duplicate_edit {bool history_targets(const a::Binding&,const a::Request&,std::map<uint64_t,std::string>&,std::string& e){return fail(e,"Unexpected duplicate");}}
namespace group_edit {bool history_admitted(const a::Binding&,const a::Request&,std::string& e){return fail(e,"Unexpected group");}}
}
namespace action=mt2mp::action_session;
struct EngineFixture {
    struct Source {a::Request request;};std::map<std::string,Source> buildings_,operations_;
    bool live_state_=false,live_port_=false,live_result_=false;unsigned routed=0;
    a::Outcome execute_action_native(const a::Binding&,const a::Request&,std::string&){++routed;return a::Outcome::Confirmed;}
    a::Outcome execute(const a::Binding& binding,const a::Request& request,std::string& error){
        #include "character_rename_engine_actual.inc"
        return a::Outcome::RejectedNoEffect;
    }
};
bool pending_ui(){return pending.has_value()||replaying||preparing_view||rename_tail.armed||diagnostic_pending();}
#ifndef CHARACTER_RENAME_UI_FIXTURE
bool diagnostic_pending(){return false;}
bool diagnostic_submitted(const std::string&,const std::string&,std::string&){return true;}
void diagnostic_retire_lobby(){}
#endif
void reset(){
    capture_dispatch={};fixture_records.clear();
    native_text.clear();native_values.clear();
    original_count=seed_count=submit_count=original_calls=begin_rename_count=drop_focus_count=selection_rebuilds=0;preparation_notifications=0;rejected=0;rename_tail={};preparing_view=false;captures=submitted=0;ready=true;fault=replaying=false;
    scoped=scope_valid=binding_valid=session_valid=true;original_go=false;
    preparation_rng=preparation_effect=bad_effect=bad_ledger=bad_owner=read_failed=false;original_returned=cleanup=true;
    Config::instance().is_host=true;session={};session.role=a::Role::Host;session.session=5;session.local_peer=1;session.host_peer=1;session.peer_mask=3;session.snapshot_uuid=fixture_snapshot;
    fixture_request={};fixture_request.action=a::Action::RenameCharacter;fixture_request.action_uuid="11111111-1111-4111-8111-111111111111";
    fixture_request.peer=1;fixture_request.snapshot_uuid=fixture_snapshot;fixture_request.character_rename={0,1,"Old name","New name"};fixture_request.canonical_record_key=a::character_key(fixture_request.character_rename);
    fixture_controller={{"ordered",true},{"started",true},{"locked",false},{"pending",true},{"action_uuid",fixture_request.action_uuid}};
    ui_selected=0x400;ui_text="New name";ui_numeric_cache={3,4};ui_selection.fill(0xffffffffu);ui_focus=0;ui_editing=ui_selectable=0;
    ui_input_cache="Input cache";ui_commit_baseline="Commit baseline";ui_disabled=1;route_valid=true;bound_button=restore_failed=drop_focus_noop=false;
    active_ui_token.clear();diagnostic_retire_lobby();pending.reset();operations.clear();cause.clear();last_action.clear();live={};
    live.proof={{"manager_sha256",std::string(64,'a')},{"family_sha256",std::string(64,'b')},{"actors_sha256",std::string(64,'c')},
        {"bosses_sha256",std::string(64,'d')},{"release_sha256",std::string(64,'e')},{"other_definitions_sha256",std::string(64,'f')},
        {"other_actors_sha256",std::string(64,'a')},{"quests_sha256",std::string(64,'b')},{"art_store_sha256",std::string(64,'c')}};
    live.value.name=live.value.definition_name="Old name";live.value.day=28;live.value.rename_day=20;live.value.count=13;
    live.value.released=true;live.value.all_sha256=std::string(64,'a');live.value.others_sha256=std::string(64,'b');live.value.global_random={11,12,13,14};
}
#if !defined(CHARACTER_RENAME_UI_FIXTURE) && !defined(CHARACTER_RENAME_NO_MAIN)
int main()try{
    std::string error;a::Evidence host;
    reset();fixture_controller["pending"]=false;original_go=true;
    FixtureRecord text_commit{"DoRename",{"New name"}};fixture_records.emplace(&text_commit,&text_commit);bool whole_record_match=true;
    need(!quest_list::label(0,&text_commit,"DoRename",whole_record_match)&&!whole_record_match,
        "Whole-record comparator fixture did not preserve native argument-count mismatch");fixture_records.erase(&text_commit);
    need(dispatch_fixture_record(text_commit)&&pending&&pending->intent.name=="New name"&&captures==1&&!original_count&&
        capture_dispatch.entries==1&&capture_dispatch.rename_labels==1&&capture_dispatch.disposition=="captured",
        "Argument-bearing native DoRename bypassed actual capture body");
    reset();fixture_controller["pending"]=false;original_go=true;
    need(dispatch_fixture_record({"Unrelated",{"DoRename"}})&&!captures&&capture_dispatch.rename_labels==0&&
        capture_dispatch.disposition=="unrelated-label-original","Argument value was mistaken for the native command label");
    for(bool is_host:{true,false}){
        reset();Config::instance().is_host=is_host;session.role=is_host?a::Role::Host:a::Role::Client;session.local_peer=is_host?1:2;fixture_request.peer=session.local_peer;
        fixture_controller["pending"]=false;original_go=true;
        need(entry(reinterpret_cast<void*>(0x100),const_cast<char*>("DoRename")),error);
        need(pending&&pending->context==session&&captures==1&&!original_count,"Original input mutated before ordered consume");
        original_go=false;tick();need(submit_count==1&&submitted==1&&!fault,"Captured Rename did not submit");fixture_controller["pending"]=true;
        EngineFixture engine;need(engine.execute(expected_binding,fixture_request,error)==a::Outcome::Confirmed&&engine.routed==1,error);
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed,error);
        need(original_count==1&&!seed_count&&operations.at(fixture_request.action_uuid).ordered,"Ordered original count/RNG differs");
        a::Evidence observed;need(observe(expected_binding,fixture_request,observed,error),error);
        if(is_host)host=observed;else need(host==observed,"Complete peer effects differ");
        need(retire(expected_binding,fixture_request,error)&&observe(expected_binding,fixture_request,observed,error)&&observed.retirement_confirmed,error);
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&original_count==1,"Confirmed Rename replayed");
        scoped=false;need(!observe(expected_binding,fixture_request,observed,error),"Observation escaped consumed scope");
    }
    for(bool is_host:{true,false}){
        reset();Config::instance().is_host=is_host;session.role=is_host?a::Role::Host:a::Role::Client;session.local_peer=is_host?1:2;fixture_request.peer=is_host?2:1;
        ui_selected=0x999;ui_text="Remote draft";const auto saved_cursor=ui_numeric_cache;
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed,error);
        const auto& op=operations.at(fixture_request.action_uuid);
        need(original_count==1&&!seed_count&&op.ui_restored&&op.presentation_calls==1&&op.preparation_notifications==0,"Remote UI scope original/tail/notification count differs");
        need(ui_selected==0x999&&ui_text=="Remote draft"&&ui_numeric_cache==saved_cursor&&ui_disabled==1,"Remote selected Character/draft/cursor/button not restored");
    }
    for(bool is_host:{true,false}){
        reset();Config::instance().is_host=is_host;session.role=is_host?a::Role::Host:a::Role::Client;session.local_peer=is_host?1:2;fixture_request.peer=is_host?2:1;
        ui_selected=0x999;ui_text="Hidden remote draft";const auto saved_cursor=ui_numeric_cache;
        native_values[{0x100,0x370}]=0;native_values[{0x200,0x278}]=0;native_values[{0x200,0x280}]=0;
        native_values[{0x700,0x278}]=0;native_values[{0x700,0x280}]=0;
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed,error);
        const auto& op=operations.at(fixture_request.action_uuid);
        need(original_count==1&&!seed_count&&!begin_rename_count&&op.ui_restored&&op.presentation_calls==1&&!op.preparation_notifications,
            "Hidden remote cached editor opened UI, adopted RNG or replayed original");
        need(ui_selected==0x999&&ui_text=="Hidden remote draft"&&ui_numeric_cache==saved_cursor,
            "Hidden remote cached editor did not restore target/draft/value state");
    }
    for(unsigned shape=0;shape<3;++shape){
        reset();fixture_request.peer=2;ui_selected=0x999;ui_text="Remote inactive draft";
        if(shape==1)ui_selection={};
        if(shape==2)ui_selection={0,0xffffffffu,0,0xffffffffu,0,0xffffffffu,0,0xffffffffu};
        const auto native_selection=ui_selection;
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed,error);
        need(ui_selection==native_selection&&selection_rebuilds==1&&!ui_focus&&!ui_editing&&!drop_focus_count&&!captures,
            "Native constructor/cleared/LostFocus inactive selection shape was rejected or normalized");
    }
    for(unsigned variant=0;variant<4;++variant){
        reset();fixture_request.peer=2;
        if(variant==0)native_values[{0x780,0}]=0x201;
        if(variant==1)native_values[{0x200,0x278}]=0x100;
        if(variant==2)native_values[{0x200,0x280}]=0x999;
        if(variant==3)native_text[{0x700,0x1b0}]="Foreign name pages";
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&!original_count&&!seed_count,
            "Foreign cached page/name/live ancestry entered original DoRename");
    }
    reset();fixture_request.peer=2;ui_selected=0x999;ui_text="Long remote draft";
    fixture_request.character_rename.name="Hi";fixture_request.canonical_record_key=a::character_key(fixture_request.character_rename);
    ui_focus=0x200;ui_editing=1;ui_selection={11,0,3,0,3,0,11,0};ui_numeric_cache={0xdeadbeef,0x7f800001};
    const auto saved_selection=ui_selection;const auto saved_numeric=ui_numeric_cache;
    const auto saved_input=ui_input_cache,saved_baseline=ui_commit_baseline;
    need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed,error);
    need(ui_text=="Long remote draft"&&ui_selection==saved_selection&&ui_numeric_cache==saved_numeric&&selection_rebuilds==1&&
        ui_focus==0x200&&ui_editing==1&&ui_input_cache==saved_input&&ui_commit_baseline==saved_baseline&&!drop_focus_count&&
        !operations.at(fixture_request.action_uuid).preparation_notifications&&!captures,"Short remote name lost real selection/numeric/input/baseline/focus state or committed local UI");
    for(unsigned variant=0;variant<4;++variant){
        reset();fixture_request.peer=2;
        if(variant==0)native_text[{0x200,0x400}]="MessageTo Parent DoRename";
        if(variant==1)native_text[{0x200,0x3e0}]="";
        if(variant==2)native_values[{0x200,0x4a0}]=0x999;
        if(variant==3)ui_focus=0x200;
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&!original_count&&!drop_focus_count&&!captures,
            "Preview/commit/bound-value/inconsistent focus contract entered original");
    }
    reset();bound_button=true;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&!original_count,"Bound UI button entered original");
    reset();route_valid=false;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&!original_count,"Foreign presentation hook entered original");
    for(bool is_host:{true,false})for(unsigned ui_change=0;ui_change<3;++ui_change){
        reset();Config::instance().is_host=is_host;session.role=is_host?a::Role::Host:a::Role::Client;
        session.local_peer=is_host?1:2;fixture_request.peer=session.local_peer;
        ui_commit_baseline=fixture_request.character_rename.name;
        if(ui_change==0){ui_text=live.value.name;ui_selection={0,0xffffffffu,0,0xffffffffu,0,0xffffffffu,0,0xffffffffu};}
        else {ui_text="Later local draft";ui_focus=0x200;ui_editing=1;ui_selection={1,0,3,0,1,0,3,0};}
        if(ui_change==2)ui_selected=0x999;
        Memory memory;OrderedView current_ui;need(read_ordered_view(memory,0x100,current_ui,error),error);
        const auto saved_ui=ordered_view_state(current_ui);
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Confirmed,error);
        const auto& op=operations.at(fixture_request.action_uuid);
        need(original_count==1&&!seed_count&&!captures&&op.ui_restored&&op.ui_state.at("origin")==true&&
            op.ui_state.at("before")==saved_ui&&op.ui_state.at("restored")==saved_ui&&
            op.ui_state.at("current_target_matches")==bool(ui_change!=2)&&op.ui_state.at("current_draft_matches")==false&&
            op.ui_state.at("prepared").at("selected")==0x400&&op.ui_state.at("prepared").at("text")=="New name"&&
            live.value.name=="New name","Deferred origin command depended on transient UI or failed exact local restoration");
        need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&original_count==1,
            "Deferred origin command replayed after original confirmation");
    }
    reset();fixture_request.peer=2;restore_failed=true;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&original_count==1&&!operations.at(fixture_request.action_uuid).ui_restored,"Failed UI restore confirmed");
    reset();live.value.name="Concurrent name";ui_text="Later local draft";
    need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::RejectedNoEffect&&!original_count&&!seed_count&&
        ui_text=="Later local draft"&&error=="Character rename recorded prior name differs measured=Concurrent name expected=Old name",
        "Transient UI independence admitted changed shared precondition or lost measured prior name");
    auto check_rejected=[&](auto change){reset();change();need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::RejectedNoEffect,error);need(!original_count&&!seed_count,"Rejected Rename changed gameplay");};
    check_rejected([]{scoped=false;});check_rejected([]{scope_valid=false;});check_rejected([]{session.role=a::Role::Client;});
    check_rejected([]{fixture_request.action=a::Action::SetCharacterHealth;fixture_request.character_rename.property=1;});
    check_rejected([]{fixture_controller["pending"]=false;});check_rejected([]{fixture_controller["locked"]=true;});
    reset();a::Evidence foreign;need(perform(expected_binding,fixture_request,&foreign,error)==a::Outcome::RejectedNoEffect&&!original_count&&!seed_count,"Host evidence adopted");
    for(auto fault_flag:{&preparation_rng,&preparation_effect,&bad_effect,&bad_ledger,&bad_owner,&read_failed}){
        reset();fixture_request.peer=2;session.local_peer=1;ui_selected=0x999;ui_text="Remote draft";*fault_flag=true;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown,"Foreign full effect accepted");
        const auto count=original_count;need(!seed_count&&perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&original_count==count,"Unknown Rename replayed");
    }
    reset();original_returned=false;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&original_count==1,"Failed original return accepted");
    reset();cleanup=false;need(perform(expected_binding,fixture_request,nullptr,error)==a::Outcome::Unknown&&original_count==1,"Failed cleanup accepted");
    reset();pending=Pending{expected_binding,fixture_snapshot,fixture_request.character_rename,session};++session.session;tick();
    need(!submit_count&&fault&&cause=="Ordered Character captured Session changed","Changed Session submitted");
    reset();fixture_controller["pending"]=false;need(!entry(reinterpret_cast<void*>(0x100),const_cast<char*>("DoRename"))&&!pending&&!original_count,"Capture outside OriginalGo admitted");
    reset();EngineFixture engine;engine.live_state_=true;need(engine.execute(expected_binding,fixture_request,error)==a::Outcome::RejectedNoEffect&&!engine.routed,"State-copy bridge admitted");
    reset();auto& complete=character_membership::complete;complete={};complete.value.global_random=live.value.global_random;
    complete.value.import_release_sha="complete Release log";complete.value.manager_sha="manager";complete.value.family_sha="selected definition";complete.value.others_sha="other definitions";
    complete.value.users_sha="all actor records";complete.other_users_sha="other actor records";complete.value.quests_sha="all Quest records";complete.value.import_art_sha="complete ArtStore";
    Memory memory;auto proof_sample=live;
    need(complete_proof(memory,expected_binding,fixture_snapshot,fixture_request.character_rename,proof_sample,json::array(),json::array(),error),error);
    need(character_membership::read_only_requested,"Complete proof requested mutation semantics");
    const auto proof=proof_sample.value;
    need(proof.all_sha256.find("all actor records")!=std::string::npos&&proof.all_sha256.find("selected definition")!=std::string::npos&&proof.all_sha256.find("complete Release log")!=std::string::npos,"Complete affected records absent");
    need(proof.others_sha256.find("other actor records")!=std::string::npos&&proof.others_sha256.find("all Quest records")!=std::string::npos&&proof.others_sha256.find("complete ArtStore")!=std::string::npos,"Complete unaffected records absent");
    complete.value.users_sha="changed selected actor record";proof_sample=live;
    need(complete_proof(memory,expected_binding,fixture_snapshot,fixture_request.character_rename,proof_sample,json::array(),json::array(),error)&&
        proof_sample.value.all_sha256!=proof.all_sha256&&proof_sample.value.others_sha256==proof.others_sha256,"Original affected actor name refresh suppressed or made invariant");
    ++complete.value.global_random[0];proof_sample=live;
    need(!complete_proof(memory,expected_binding,fixture_snapshot,fixture_request.character_rename,proof_sample,json::array(),json::array(),error),"Complete proof accepted changed RNG");
    --complete.value.global_random[0];
    for(auto field:{&complete.value.others_sha,&complete.other_users_sha,&complete.value.quests_sha,&complete.value.import_art_sha}){
        const auto prior=*field;*field+=" changed";proof_sample=live;
        need(complete_proof(memory,expected_binding,fixture_snapshot,fixture_request.character_rename,proof_sample,json::array(),json::array(),error)&&
            proof_sample.value.others_sha256!=proof.others_sha256,"Unrelated complete record change escaped invariant proof");*field=prior;
    }
    reset();fixture_request.action=a::Action::SetCharacterHealth;fixture_request.character_rename.property=1;EngineFixture stat_engine;
    need(stat_engine.execute(expected_binding,fixture_request,error)==a::Outcome::RejectedNoEffect&&!stat_engine.routed,"Rename admission expanded to Character stats");
    std::cout<<checks<<" actual ordered Character Rename capture/execution/effect/no-replay checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

#endif
