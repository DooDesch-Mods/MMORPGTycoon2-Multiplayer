#include "npc_quest_list_fixture.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>

namespace a=mt2mp::action_session;
using json=nlohmann::json;
namespace fixture {
bool fail(std::string& e,const std::string& why){e=why;return false;}
struct Memory{};
std::map<std::pair<uintptr_t,size_t>,std::vector<uint8_t>> fields;
unsigned setters=0;bool mismatch=false;float slider_value=17;
template<class T>void store(uintptr_t object,size_t offset,const T& value){
    auto& bytes=fields[{object,offset}];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);}
template<class T>bool field(Memory&,uintptr_t object,size_t offset,T& value,std::string& e){
    const auto found=fields.find({object,offset});if(found==fields.end()||found->second.size()!=sizeof value)return fail(e,"Native control field absent");
    std::memcpy(&value,found->second.data(),sizeof value);return true;}
namespace npc_census{bool utf8(const std::string&){return true;}}
namespace npc_quest_records{
bool read_string(Memory&,uintptr_t,size_t,const char*,std::string&,std::string& e){return fail(e,"Title control outside fixture");}}
#include "quest_list_edit_args_actual.inc"
uint32_t external_value(uintptr_t,int32_t,float& value){value=slider_value;return 0;}
#include "quest_list_read_edit_actual.inc"
uint32_t set_edit_controls(uintptr_t,const a::NpcQuestListIntent* intent,uintptr_t first,uintptr_t second,uintptr_t third){
    ++setters;if(intent->edit==a::QuestListEdit::Kills){slider_value=float(intent->kills)+(mismatch?1.f:0.f);}
    else{store(first,0x346,uint8_t(intent->defaults));if(!intent->defaults){store(second,0x44c,intent->xp);store(third,0x44c,intent->gold+(mismatch?1:0));}}
    return 0;}
#include "quest_list_prepare_edit_actual.inc"
void reset(){fields.clear();setters=0;mismatch=false;slider_value=17;
    store(100,0x3a8,uintptr_t(200));store(200,0x4d0,int32_t(17));store(200,0x420,uintptr_t(0));store(200,0x428,uintptr_t(0));
    store(100,0x378,uintptr_t(300));store(300,0x346,uint8_t(0));store(300,0x3c0,uintptr_t(0));store(300,0x3c8,uintptr_t(0));
    store(100,0x380,uintptr_t(400));store(100,0x388,uintptr_t(500));store(400,0x44c,int32_t(400));store(500,0x44c,int32_t(31));}
json args(a::QuestListEdit edit,bool defaults=false){return {{"kind",int(edit)},{"title",""},{"kills",edit==a::QuestListEdit::Kills?17:0},
    {"xp",edit==a::QuestListEdit::Rewards&&!defaults?400:0},{"gold",edit==a::QuestListEdit::Rewards&&!defaults?31:0},{"defaults",defaults}};}
}
int main()try{
    using namespace fixture;Memory memory;std::string error;
    for(auto edit:{a::QuestListEdit::Kills,a::QuestListEdit::Rewards})for(bool defaults:{false,true}){
        if(edit==a::QuestListEdit::Kills&&defaults)continue;
        reset();a::NpcQuestListIntent intent;need(edit_args(args(edit,defaults),intent,error),error);
        need(a::ordered_quest_list(intent),"Offered edit lacks ordered scope");
        need(std::string(edit_label(edit))==(edit==a::QuestListEdit::Kills?"SetKills":"SetRewards"),"Original label differs");
        need(prepare_edit(memory,100,intent,error)&&setters==1,error);
        auto captured=intent;captured.kills=captured.xp=captured.gold=0;captured.defaults=false;
        need(read_edit(memory,100,captured,error)&&captured==intent,"Original native controls lost captured values");
        if(!defaults){reset();mismatch=true;need(!prepare_edit(memory,100,intent,error)&&setters==1,"Incorrect native control readback accepted");}
        reset();store(edit==a::QuestListEdit::Kills?200:300,edit==a::QuestListEdit::Kills?0x420:0x3c0,uintptr_t(999));
        need(!prepare_edit(memory,100,intent,error)&&setters==0,"Bound gameplay control mutated before execution");
    }
    reset();a::NpcQuestListIntent intent;intent.edit=a::QuestListEdit::Kills;slider_value=std::numeric_limits<float>::quiet_NaN();
    need(!read_edit(memory,100,intent,error),"Nonfinite Slider accepted");
    reset();intent.edit=a::QuestListEdit::Rewards;store(300,0x346,uint8_t(2));need(!read_edit(memory,100,intent,error),"Invalid native checkbox accepted");
    reset();store(400,0x44c,int32_t(-1));need(!read_edit(memory,100,intent,error),"Negative custom reward accepted");
    for(auto invalid:{json{{"kind",2},{"title",""},{"kills",-1},{"xp",0},{"gold",0},{"defaults",false}},
                      json{{"kind",3},{"title",""},{"kills",0},{"xp",1},{"gold",0},{"defaults",true}}}){
        need(!edit_args(invalid,intent,error),"Cross-command or out-of-domain edit accepted");}
    std::cout<<"Ordered Quest Kills and Rewards original control bodies passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
