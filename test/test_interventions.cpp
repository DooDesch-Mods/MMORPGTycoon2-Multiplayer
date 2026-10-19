#include "intervention_native.h"
#include "intervention_actor_native.h"
#include <iostream>
#include <stdexcept>
namespace game=mt2mp::game;
namespace i=game::interventions;
using Json=nlohmann::json;
namespace {
unsigned checks=0,calls=0;uintptr_t receiver=0;std::string command;
void need(bool value,const std::string& error){++checks;if(!value)throw std::runtime_error(error);}
const std::string uuid="11111111-2222-3333-4444-555555555555",sha(64,'a');
i::Target target(i::Family family){switch(family){case i::Family::Boss:return {family,sha+":world:boss:4"};case i::Family::Monster:return {family,"action:"+uuid};case i::Family::Npc:return {family,"action:"+uuid};case i::Family::Toon:return {family,"subscriber:17"};}throw std::runtime_error("unknown fixture family");}
i::Proof proof(i::Family family){i::Proof out;out.target=target(family);out.actor={sha,5,6,2};if(family==i::Family::Toon)out.subscriber=out.actor;out.actors_sha=out.zones_sha=out.economy_sha=out.gm_sha=out.marketing_sha=out.tutorial_sha=sha;return out;}
bool original(void* object,const void* text){struct String{const char* data;size_t size;};const auto& native=*static_cast<const String*>(text);++calls;receiver=uintptr_t(object);command.assign(native.data,native.size);return true;}
template<class Read>void rejected(const Json& wire,Read read){bool failed=false;try{(void)read(wire);}catch(const std::exception&){failed=true;}need(failed,"malformed intervention packet accepted");}
void domain(){for(auto family:{i::Family::Boss,i::Family::Monster,i::Family::Npc,i::Family::Toon}){
    auto before=proof(family);need(i::valid(before),"complete original proof rejected");need(i::codec::proof(i::encode(before))==before,"complete proof roundtrip changed values");
    for(auto kind:{i::Command::Heal,i::Command::Kill,i::Command::Warn,i::Command::Ban,i::Command::Respawn,i::Command::RaiseLevel,i::Command::LowerLevel}){
        i::Intent request{target(family),kind,sha};const auto allowed=family==i::Family::Toon||kind==i::Command::Heal||kind==i::Command::Kill;
        need(i::valid(request)==allowed,"original command family constraint differs");if(!allowed)continue;
        need(i::codec::intent(i::encode(request))==request,"command request roundtrip changed values");need(i::effect(request,before,before),"valid original no-op was lost");
        auto after=before;after.actor.sha=std::string(64,'b');need(i::effect(request,before,after),"original complete record effect rejected");
        auto stale=request;stale.before_sha=std::string(64,'b');need(!i::effect(stale,before,after),"stale original before-record accepted");
        after.target.key="subscriber:18";need(!i::effect(request,before,after),"command result was rebound to another target");}
    auto missing=before;missing.subscriber.reset();need(i::valid(missing)==(family!=i::Family::Toon),"Toon Subscriber proof presence differs");
    if(family!=i::Family::Toon){missing.subscriber=before.actor;need(!i::valid(missing),"foreign Subscriber witness accepted");}
    for(auto member:{&i::Proof::actors_sha,&i::Proof::zones_sha,&i::Proof::economy_sha,&i::Proof::gm_sha,&i::Proof::marketing_sha,&i::Proof::tutorial_sha}){auto partial=before;(partial.*member).clear();need(!i::valid(partial),"partial native cascade proof accepted");}}
    need(i::zone_monster_key(sha+":world:zone:3:monster:0"),"saved Zone-owned current Monster rejected");need(i::zone_monster_key("action:"+uuid+":monster:13"),"ordered new Zone-owned Monster rejected");
    need(!i::zone_monster_key("action:"+uuid+":monster:01"),"ambiguous current Zone member index accepted");need(!i::zone_monster_key("action:"+uuid+":monster:-1"),"negative current Zone member index accepted");
    need(i::boss_key("action:"+uuid+":boss:0"),"ordered original Boss ordinal0 rejected");need(!i::boss_key("action:"+uuid+":boss:00"),"ambiguous Boss birth ordinal accepted");
    need(i::boss_key(sha+":world:boss:"+game::snapshot_boss_key::root_suffix(3,2)),"original inline Root Boss key rejected");
    for(int instance:{-1,0,12})need(i::boss_key(sha+":world:boss:"+game::snapshot_boss_key::scene_suffix(4,5,instance,6)),"original template/instance Boss key rejected");
}
void codec(){const auto request=i::Intent{target(i::Family::Toon),i::Command::Warn,sha};const auto wire=i::encode(request);const auto state=i::encode(proof(i::Family::Toon));
    for(const auto& value:{Json(true),Json(-1),Json(1.25),Json(uint64_t(UINT32_MAX)+1),Json(0),Json(8)}){auto bad=wire;bad["command"]=value;rejected(bad,i::codec::intent);}
    auto bad=wire;bad["target"]["family"]=4;rejected(bad,i::codec::intent);bad=wire;bad["target"]["key"]="subscriber:017";rejected(bad,i::codec::intent);
    bad=wire;bad["target"]["key"]="subscriber:2147483648";rejected(bad,i::codec::intent);bad=wire;bad["target"]["key"]="subscriber:17\0foreign";bad["target"]["key"]=std::string("subscriber:17\0foreign",21);rejected(bad,i::codec::intent);
    for(const char* field:{"target","actor","subscriber","actors_sha","zones_sha","economy_sha","gm_sha","marketing_sha","tutorial_sha"}){bad=state;bad.erase(field);rejected(bad,i::codec::proof);}
    bad=state;bad["unknown"]=0;rejected(bad,i::codec::proof);bad=state;bad["actor"]["records"]=0;rejected(bad,i::codec::proof);bad=state;bad["actor"]["tokens"]=262145;rejected(bad,i::codec::proof);
    bad=state;bad["subscriber"]=nullptr;rejected(bad,i::codec::proof);bad=state;bad["actor"]["sha"]=std::string(64,'A');rejected(bad,i::codec::proof);
}
void native(){for(auto kind:{i::Command::Heal,i::Command::Kill,i::Command::Warn,i::Command::Ban,i::Command::Respawn,i::Command::RaiseLevel,i::Command::LowerLevel}){
    const auto before=calls;const auto result=i::native::execute(&original,0x1234,kind);need(result.attempted&&result.returned&&result.result&&!result.exception&&calls==before+1&&receiver==0x1234&&command==i::original_command(kind),"actual original Command call/borrowed GNU string was not once");}
    const auto before=calls;for(const auto result:{i::native::execute(nullptr,0x1234,i::Command::Kill),i::native::execute(&original,0,i::Command::Kill),i::native::execute(&original,0x1234,i::Command(0))})need(!result.attempted&&!result.returned&&result.exception&&calls==before,"invalid original command was attempted");
    alignas(uintptr_t) uintptr_t link=0x12345678abcdef00;bool exchanged=true;const auto address=uintptr_t(&link);
    need(!i::actors::native::compare_pointer(address,link,0xfedcba9876543210,exchanged)&&exchanged&&link==0xfedcba9876543210,"actual ObjectLink CAS truncated native pointer/high bits");
    need(!i::actors::native::compare_pointer(address,0x12345678abcdef00,0,exchanged)&&!exchanged&&link==0xfedcba9876543210,"actual ObjectLink CAS overwrote a changed physical owner");
    need(i::actors::native::compare_pointer(address+1,link,0,exchanged)&&!exchanged&&link==0xfedcba9876543210,"misaligned ObjectLink CAS attempted a native write");
    need(i::actors::native::compare_pointer(0,0,1,exchanged)&&!exchanged,"null ObjectLink CAS attempted a native write");
}
}
int main()try{domain();codec();native();std::cout<<checks<<" intervention canonical command, complete record proof, strict packet and original once assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
