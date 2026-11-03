#define main closed_token_label_regression
#include "test_token_label_capture.cpp"
#undef main
namespace followup {
using namespace token_capture_fixture;
namespace quest {
struct Hook {a::ui_diagnostic::UiCommand original=&fixture_original;}npc_hook,selector_hook;
bool fault=false,replaying=false,replay_remove=true;
uintptr_t replay_receiver=0x600;unsigned original_calls=0,selected=0,rejected=0;
a::QuestListEdit replay_edit=a::QuestListEdit::Title,selected_edit=a::QuestListEdit::List;
std::string cause;
bool reject(const std::string& error){++rejected;cause=error;return false;}
using quest_list::label;
#include "quest_command_gate_actual.inc"
}
namespace region {
struct NativeString {const char* data;size_t length;unsigned char rest[16]{};};
struct Runtime {uintptr_t image=state.image;};unsigned selected=0;
uint32_t label_is(uintptr_t,void* record,const char* label,bool& match){match=fixture_record_equals(record,label);return 0;}
#include "selection_label_actual.inc"
#include "appearance_command_gate_actual.inc"
}
namespace dungeon {
#include "dungeon_tab_command_actual.inc"
bool full_record(const void* record,const char* expected){
    const auto& value=*static_cast<const FixtureRecord*>(record);
    if(value.arguments.size()!=1)return false;
    return std::string(expected)==value.label+" \""+value.arguments[0].text+"\"";
}
}
}
int main()try{
    using namespace token_capture_fixture;using namespace followup;
    unsigned checks=0;auto check=[&](bool value,const char* why){++checks;if(!value)throw std::runtime_error(std::string(why)+" cause="+quest::cause);};
    FixtureRecord title{"SetTitle",{{1,"Real edited quest title",0}}};fixture_bind(title);
    check(!fixture_record_equals(&title,"SetTitle"),"Full-record mismatch premise absent");
    check(quest::capture(true,reinterpret_cast<void*>(0x600),&title)&&quest::selected==1&&quest::selected_edit==a::QuestListEdit::Title&&!fixture_originals,"Original TextEdit command bypassed ordered Quest admission");
    quest::replaying=true;check(quest::capture(true,reinterpret_cast<void*>(0x600),&title)&&quest::original_calls==1&&fixture_originals==1,"Consumed argument-bearing Title did not execute once");
    check(!quest::capture(true,reinterpret_cast<void*>(0x600),&title)&&quest::original_calls==1&&fixture_originals==1,"Consumed Title executed twice");quest::replaying=false;quest::fault=false;
    title.label="Other";title.arguments[0].text="SetTitle";const auto before=fixture_originals;
    check(quest::capture(true,reinterpret_cast<void*>(0x600),&title)&&quest::selected==1&&fixture_originals==before+1,"Argument text became the Quest command label");
    title.label="SetTitle";fixture_hash_mismatch=true;const auto selected=quest::selected,forwarded=fixture_originals;
    check(!quest::capture(true,reinterpret_cast<void*>(0x600),&title)&&quest::selected==selected&&fixture_originals==forwarded&&quest::fault,"Bad native token pin allowed Title mutation/forwarding");fixture_hash_mismatch=false;quest::fault=false;
    fixture_native_exception=true;check(!quest::capture(true,reinterpret_cast<void*>(0x600),&title)&&fixture_originals==forwarded,"Unreadable Title label forwarded into original mutation");fixture_native_exception=false;
    region::Runtime runtime;std::string error;bool handled=false;
    FixtureRecord fx{"FXChange",{{1,"Snow",0}}};check(!fixture_record_equals(&fx,"FXChange"),"Combo callback record unexpectedly equals bare label");
    check(region::appearance_capture(runtime,nullptr,&fx,handled,error)&&handled&&region::selected==1,"Actual Combo selection failed Region admission");
    fx.label="Other";fx.arguments[0].text="FXChange";handled=true;
    check(region::appearance_capture(runtime,nullptr,&fx,handled,error)&&!handled&&region::selected==1,"Region argument text became command label");
    fx.label="FXChange";fixture_native_exception=true;handled=false;
    check(!region::appearance_capture(runtime,nullptr,&fx,handled,error)&&!handled&&!error.empty(),"Region token exception became successful admission");fixture_native_exception=false;
    bool match=true;check(region::selection_label_is(runtime.image,nullptr,"FXChange",match)==ERROR_INVALID_PARAMETER&&!match,"Null Region record was accepted");
    const std::string oversized(65,'x');match=true;check(region::selection_label_is(runtime.image,&fx,oversized.c_str(),match)==ERROR_INVALID_PARAMETER&&!match,"Unbounded expected Region label accepted");
    state.image=uintptr_t(&dungeon::full_record)-0x1113d80;
    FixtureRecord page{"SetPage",{{1,"{dungeonconfig_instances}",0}}};
    check(dungeon::tab_record_equals(&page,"SetPage \"{dungeonconfig_instances}\""),"Dungeon generated page argument was discarded");
    check(!dungeon::tab_record_equals(&page,"SetPage \"{other_page}\""),"Dungeon full-record confirmation ignored actual page argument");
    check(!dungeon::tab_record_equals(&page,"SetPage"),"Dungeon argument-bearing command matched bare record");
    page.label="Other";check(!dungeon::tab_record_equals(&page,"SetPage \"{dungeonconfig_instances}\""),"Dungeon wrong label accepted");
    std::cout<<checks<<" actual command-label follow-up checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
