#include "typed_action/action_session.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include "dungeon_sound_record.h"
#include "dungeon_gizmo_record.h"
#include <windows.h>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace token_capture_fixture {
namespace a=mt2mp::action_session;
namespace dungeon_sound_record=mt2mp::game::dungeon_sound_record;
namespace dungeon_gizmo_record=mt2mp::game::dungeon_gizmo_record;
struct FixtureToken {int32_t type=4;std::string text;int32_t number=0;};
struct FixtureRecord {std::string label;std::vector<FixtureToken> arguments;};
namespace quest_list {struct QuestTitleView {const char* data;uint64_t size,capacity=0,padding=0;};}
bool fixture_token_equals(const void* record,const quest_list::QuestTitleView& wanted);
struct FixtureState {uintptr_t image=uintptr_t(&fixture_token_equals)-0x96e1d0;} fixture_state;
auto& state=fixture_state;
bool fixture_connected=true,fixture_copy_failure=false,fixture_hash_failure=false,fixture_hash_mismatch=false,fixture_api_failure=false,fixture_native_exception=false;
unsigned fixture_originals=0,fixture_pin_reads=0,fixture_checks=0;
uintptr_t fixture_next_tokens=0x1000000;
std::map<std::pair<uintptr_t,size_t>,uint64_t> fixture_values;
std::map<std::pair<uintptr_t,size_t>,std::string> fixture_strings;
constexpr const char* exe_sha="fixture";
bool fail(std::string& error,const std::string& why){error=why;return false;}
struct Memory final:a::ui_capture::Memory {
    bool copy(uintptr_t address,void* output,size_t bytes,std::string& error)override{
        ++fixture_pin_reads;
        if(fixture_copy_failure)return fail(error,"fixture exact token pin read failed");
        if(address!=state.image+0x96e1d0||bytes!=80)return fail(error,"fixture token pin extent/address differs");
        std::memset(output,'p',bytes);return true;
    }
    bool region(uintptr_t,a::ui_capture::Region&,std::string& error)override{return fail(error,"fixture unexpected native region query");}
};
namespace quest_bridge {
bool sha256(const std::string& bytes,std::string& output,std::string& error){
    if(fixture_hash_failure)return fail(error,"fixture exact token digest failed");
    output=!fixture_hash_mismatch&&bytes==std::string(80,'p')?"8804bc96497105c4fc82d693ffc88533c3e6c1b9e70b92e3d310cdad72a0f2cc":std::string(64,'0');return true;
}
}
#include "command_token_label_actual.inc"
bool fixture_token_equals(const void* record,const quest_list::QuestTitleView& wanted){
    if(fixture_native_exception)RaiseException(0xe0000042,0,0,nullptr);
    return static_cast<const FixtureRecord*>(record)->label==std::string_view(wanted.data,size_t(wanted.size));
}
bool fixture_record_equals(const void* record,const char* wanted){
    const auto& value=*static_cast<const FixtureRecord*>(record);
    return value.arguments.empty()&&value.label==wanted;
}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& output,std::string& error){
    const auto found=std::find_if(fixture_values.begin(),fixture_values.end(),[&](const auto& item){return item.first.first+item.first.second==object+offset;});
    if(found==fixture_values.end())return fail(error,"fixture exact argument field absent address="+std::to_string(object+offset));
    output=T(found->second);return true;
}
namespace npc_quest_records {
bool read_string(Memory&,uintptr_t object,size_t offset,const char*,std::string& output,std::string& error){
    const auto found=fixture_strings.find({object,offset});if(found==fixture_strings.end())return fail(error,"fixture exact argument text absent");output=found->second;return true;
}
}
namespace net {namespace session_wire {bool connected(){return fixture_connected;}}}
namespace quest_list {
uint32_t label(a::ui_diagnostic::RecordEquals equals,void* record,const char* wanted,bool& output){output=equals(record,wanted);return 0;}
}
void fixture_bind(FixtureRecord& value){
    const auto record=uintptr_t(&value),tokens=fixture_next_tokens;fixture_next_tokens+=0x1000;
    fixture_values[{record,0x38}]=value.arguments.empty()?0:tokens;
    fixture_values[{record,0x40}]=fixture_values[{record,0x44}]=value.arguments.size();
    for(size_t index=0;index<value.arguments.size();++index){const auto token=tokens+index*0x30;
        fixture_values[{token,0}]=uint64_t(value.arguments[index].type);
        fixture_values[{token,0x28}]=uint64_t(value.arguments[index].number);
        fixture_strings[{token,8}]=value.arguments[index].text;}
}
bool fixture_original(void*,void*){++fixture_originals;return true;}
void fixture_need(bool value,const std::string& why){++fixture_checks;if(!value)throw std::runtime_error(why);}

namespace character {
unsigned fixture_queued=0,fixture_rejected=0;
bool reject(const std::string&){++fixture_rejected;return false;}
bool queue(uintptr_t owner,a::CharacterMembershipCommand command,std::string& error){
    if(owner!=0x600||command!=a::CharacterMembershipCommand::CreateVariant)return fail(error,"fixture exact variant owner/command differs");++fixture_queued;return true;
}
#include "character_variant_capture_actual.inc"
}
namespace ability {
using Command=a::AbilityMembershipCommand;
bool fault=false,executing=false;
auto original_new=&fixture_original;
unsigned fixture_queued=0,fixture_rejected=0;
int32_t fixture_level=-1;
std::string fixture_cause;
bool reject(const std::string& why){++fixture_rejected;fixture_cause=why;return false;}
bool queue(uintptr_t owner,a::AbilityMembershipIntent intent,uintptr_t source,std::string& error){
    if(owner!=0x700||intent.command!=Command::Create||source)return fail(error,"fixture exact ability owner/command differs");++fixture_queued;fixture_level=intent.level;return true;
}
#include "ability_new_capture_actual.inc"
}
namespace sound {
bool fault=false,replaying=false,preparing=false;
uintptr_t replay_receiver=0x600;
int32_t replay_atmosphere=2;
uint32_t original_calls=0;
auto original=&fixture_original;
unsigned fixture_selected=0,fixture_rejected=0;
std::string fixture_cause;
bool reject(const std::string& why){++fixture_rejected;fixture_cause=why;return false;}
bool record_value(Memory& memory,void* record,int32_t& output,std::string& error){
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(memory,object,offset,value,cause);};
    return dungeon_sound_record::read(at,uintptr_t(record),output,error);
}
#include "dungeon_sound_replay_label_actual.inc"
#include "dungeon_sound_capture_label_actual.inc"
}
namespace gizmo {
bool fault=false,replaying=false,callback_failed=false;
a::DungeonGizmoEdit replay_edit=a::DungeonGizmoEdit::Gold;
int32_t replay_value=2;
auto original=&fixture_original;
unsigned fixture_selected=0,fixture_rejected=0,fixture_destroyed=0;
std::string fixture_cause;
bool reject(const std::string& why){++fixture_rejected;fixture_cause=why;return false;}
bool record_value(Memory& memory,void* record,int32_t& output,std::string& error){
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(memory,object,offset,value,cause);};
    return dungeon_gizmo_record::value(at,uintptr_t(record),output,error);
}
bool window(Memory&,dungeon_gizmo_record::Window& output,std::string&){output.object=0x600;output.target=0x800;output.uid=91;return true;}
namespace dungeon_input {bool capture_gizmo_destroy(uintptr_t target,uint64_t uid,std::string& error){
    if(target!=0x800||uid!=91)return fail(error,"fixture exact Destroy owner differs");++fixture_destroyed;return true;}}
#include "dungeon_gizmo_replay_label_actual.inc"
#include "dungeon_gizmo_capture_label_actual.inc"
}
}
namespace mt2mp::action_session::ui_diagnostic {
bool resolve_api(ui::Memory&,uintptr_t image,std::string_view sha,Api& output,std::string& error,Family){
    if(token_capture_fixture::fixture_api_failure){error="fixture exact original API guard failed";return false;}
    if(image!=token_capture_fixture::state.image||sha!="fixture"){error="fixture original API identity differs";return false;}
    output.label_equals=&token_capture_fixture::fixture_record_equals;return true;
}
}
int main()try{
    using namespace token_capture_fixture;
    Memory fixture_memory;std::string fixture_error;bool fixture_match=false,fixture_handled=false;
    FixtureRecord fixture_variant{"SetVariant",{{1,"new",0}}};fixture_bind(fixture_variant);
    fixture_need(!fixture_record_equals(&fixture_variant,"SetVariant"),"Complete-record fixture incorrectly ignored arguments");
    fixture_need(character::capture_variant(reinterpret_cast<void*>(0x600),&fixture_variant,fixture_handled,fixture_error)&&fixture_handled&&character::fixture_queued==1,fixture_error);
    fixture_variant.arguments[0].text="old";fixture_bind(fixture_variant);fixture_handled=true;
    fixture_need(character::capture_variant(reinterpret_cast<void*>(0x600),&fixture_variant,fixture_handled,fixture_error)&&!fixture_handled&&character::fixture_queued==1,"Non-new variant entered CreateVariant");
    fixture_variant.label="Other";fixture_variant.arguments[0].text="SetVariant";fixture_bind(fixture_variant);
    fixture_need(character::capture_variant(reinterpret_cast<void*>(0x600),&fixture_variant,fixture_handled,fixture_error)&&!fixture_handled&&character::fixture_queued==1,"Argument text became SetVariant label");
    FixtureRecord fixture_ability{"NewAbility",{{1,"Level",0},{4,"",7}}};fixture_bind(fixture_ability);
    fixture_values[{0x600,0}]=state.image+0x171ca70;fixture_values[{0x600,0x340}]=0x700;
    fixture_need(!fixture_record_equals(&fixture_ability,"NewAbility"),"NewAbility complete-record equality hid argument contradiction");
    const bool fixture_ability_captured=ability::new_entry(reinterpret_cast<void*>(0x600),&fixture_ability);
    fixture_need(fixture_ability_captured&&ability::fixture_queued==1&&ability::fixture_level==7,"Argument-bearing NewAbility did not capture full Level payload: "+ability::fixture_cause+" queued="+std::to_string(ability::fixture_queued)+" level="+std::to_string(ability::fixture_level));
    fixture_ability.arguments[0].text="Other";fixture_bind(fixture_ability);
    fixture_need(!ability::new_entry(reinterpret_cast<void*>(0x600),&fixture_ability)&&ability::fixture_queued==1,"Foreign NewAbility first argument admitted");
    fixture_ability.label="Other";fixture_ability.arguments[0].text="NewAbility";fixture_bind(fixture_ability);const auto fixture_before=fixture_originals;
    fixture_need(ability::new_entry(reinterpret_cast<void*>(0x600),&fixture_ability)&&fixture_originals==fixture_before+1&&ability::fixture_queued==1,"Unrelated Ability label did not preserve original dispatch");
    FixtureRecord fixture_sound{"SetAtmos",{{4,"",2}}};fixture_bind(fixture_sound);
    fixture_need(sound::entry(reinterpret_cast<void*>(0x600),&fixture_sound)&&sound::fixture_selected==1,"Argument-bearing SetAtmos missed original decoder");
    fixture_need(sound::replay_record_equals(&fixture_sound,"SetAtmos 2"),"SetAtmos replay rejected matching label and argument");
    fixture_sound.arguments[0].number=3;fixture_bind(fixture_sound);
    fixture_need(!sound::replay_record_equals(&fixture_sound,"SetAtmos 2"),"SetAtmos replay ignored numeric argument");
    fixture_sound.arguments[0].type=1;fixture_bind(fixture_sound);
    fixture_need(!sound::entry(reinterpret_cast<void*>(0x600),&fixture_sound)&&sound::fixture_selected==1,"SetAtmos non-integer token admitted");
    for(size_t fixture_index=0;fixture_index<dungeon_gizmo_record::labels.size();++fixture_index){
        FixtureRecord fixture_record{dungeon_gizmo_record::labels[fixture_index],{{4,"",2}}};fixture_bind(fixture_record);
        gizmo::replay_edit=a::DungeonGizmoEdit(fixture_index);const auto fixture_selected_before=gizmo::fixture_selected;
        fixture_need(!fixture_record_equals(&fixture_record,dungeon_gizmo_record::labels[fixture_index]),"Gizmo whole-record fixture ignored arguments");
        fixture_need(gizmo::entry(reinterpret_cast<void*>(0x600),&fixture_record)&&gizmo::fixture_selected==fixture_selected_before+1,"Argument-bearing Gizmo label missed original value decoder");
        fixture_need(gizmo::replay_record_equals(&fixture_record,"ignored construction string"),"Gizmo replay rejected exact label and argument");
        fixture_record.arguments[0].number=3;fixture_bind(fixture_record);
        fixture_need(!gizmo::replay_record_equals(&fixture_record,"ignored construction string"),"Gizmo replay ignored expected argument");
        fixture_record.label="Other";fixture_record.arguments[0].text=dungeon_gizmo_record::labels[fixture_index];fixture_bind(fixture_record);const auto fixture_original_before=fixture_originals;
        fixture_need(gizmo::entry(reinterpret_cast<void*>(0x600),&fixture_record)&&fixture_originals==fixture_original_before+1&&gizmo::fixture_selected==fixture_selected_before+1,"Gizmo argument text became a numeric command label");
    }
    FixtureRecord fixture_destroy{"DestroyGizmo",{}};fixture_bind(fixture_destroy);
    fixture_need(gizmo::entry(reinterpret_cast<void*>(0x600),&fixture_destroy)&&gizmo::fixture_destroyed==1,"Bare DestroyGizmo dispatch changed");
    fixture_destroy.arguments.push_back({4,"",2});fixture_bind(fixture_destroy);const auto fixture_destroy_originals=fixture_originals;
    fixture_need(gizmo::entry(reinterpret_cast<void*>(0x600),&fixture_destroy)&&gizmo::fixture_destroyed==1&&fixture_originals==fixture_destroy_originals+1,"Argument-bearing DestroyGizmo bypassed retained whole-record comparison");
    for(unsigned fixture_failure=0;fixture_failure<4;++fixture_failure){
        fixture_copy_failure=fixture_failure==0;fixture_hash_failure=fixture_failure==1;fixture_hash_mismatch=fixture_failure==2;fixture_native_exception=fixture_failure==3;
        fixture_error.clear();fixture_match=true;
        fixture_need(!command_token_label::read(fixture_memory,&fixture_sound,"SetAtmos",fixture_match,fixture_error)&&!fixture_match&&!fixture_error.empty(),"Token-label read/pin/SEH failure became success");
    }
    fixture_copy_failure=fixture_hash_failure=fixture_hash_mismatch=fixture_native_exception=false;
    fixture_need(fixture_pin_reads>0,"Token label comparison omitted exact native body read");
    std::cout<<fixture_checks<<" actual argument-bearing token label/capture/decoder/replay checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
