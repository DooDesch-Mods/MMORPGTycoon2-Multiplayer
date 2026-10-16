#include "live_host_state/day_rollover/economy/codec.h"
#include "live_host_state/day_rollover/economy/state_difference.h"
#include <iostream>
#include <limits>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace staff=mt2mp::game::developer_staff;
namespace bug=mt2mp::game::bug_authority;
namespace quest=mt2mp::subscriber_quest_state;
using Json=nlohmann::json;
namespace {
size_t checks=0;
void need(bool ok,const std::string& e){++checks;if(!ok)throw std::runtime_error(e);}
economy::State fixture(){
    economy::State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";
    f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;
    staff::PoolState p;staff::construction::State c;
    for(int32_t i=0;i<50;++i){staff::PoolRow r;r.id=i;p.rows.push_back(r);p.indoor.push_back(i);c.actors.push_back({i,-1,{0,0,0,1},{1,1,1}});}
    bug::WorkState w;bug::JobRow j;j.bug.parent_key="action:"+f.snapshot;j.bug.birth={bug::BirthKind::Publication,f.snapshot,2,0};
    j.birth={bug::BirthKind::Publication,f.snapshot,3,0};w.jobs.push_back(j);c.work=w;p.construction=c;f.developers=p;f.bugs=bug::State{};
    s.day.history={{1,live::day::record(f.values)}};
    quest::Npc npc;npc.key="action:22222222-2222-4222-8222-222222222222";npc.level=1;
    npc.pose.region=67;npc.pose.orientation={0,0,0,1};npc.pose.scale={1,1,1};npc.quests.push_back({4,{},{},{}});s.npcs.push_back(npc);
    economy::Subscriber subscriber;subscriber.serialized_id=59;subscriber.pay={1,2,3,4,5};subscriber.play_days=6;
    quest::Toon toon;toon.gold=30;toon.level=1;toon.quests_completed=4;toon.xp_bits=std::bit_cast<uint32_t>(2.5f);
    toon.attributes={std::bit_cast<uint32_t>(1.0f),std::bit_cast<uint32_t>(2.0f),std::bit_cast<uint32_t>(3.0f)};
    toon.seconds_to_level_bits=std::bit_cast<uint32_t>(12.5f);
    toon.weapon_ilvl=12;toon.weapon_category="wands";toon.weapon_variant_id=-223771958;
    quest::Progress progress;progress.npc={quest::LinkState::Live,npc.key,-1};progress.quest={quest::LinkState::Live,npc.key,4};
    progress.completed=2;progress.monsters_defeated=3;progress.visited=true;toon.progress.push_back(progress);
    subscriber.quest_state=toon;s.subscribers.push_back(subscriber);
    economy::Subscriber no_toon;no_toon.serialized_id=63;s.subscribers.push_back(no_toon);
    s.loans.push_back({{"loan","description","owner"},{1,2,3,4}});s.offers.push_back({{"offer","description","owner"},{1,2,3}});
    return s;
}
bool decode(const Json& j,economy::State& out,std::string& e){const auto text=j.dump();return economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e);}
Json encode(const economy::State& s){std::vector<uint8_t> bytes;std::string e;need(economy::encode(s,bytes,e),"encode: "+e);return Json::parse(bytes);}
}
int main(){try{
    const auto base=fixture();auto raw=encode(base);economy::State actual;std::string e;
    need(raw["schema"]==12&&raw["day"]["frame"]["schema"]==12,"Economy/HostFrame schemas changed incorrectly");
    need(decode(raw,actual,e)&&actual==base,"complete Economy-Day-HostFrame equality: "+e);
    need(!actual.subscribers[1].quest_state,"absent Toon acquired fabricated equipment");
    const auto& row=raw["subscribers"][0]["quest_state"];
    need(row["weapon_ilvl"]==12&&row["weapon_category"]=="wands"&&row["weapon_variant_id"]==-223771958,"exact saved equipment tuple changed");
    need(std::bit_cast<uint32_t>(actual.subscribers[0].quest_state->weapon_variant_id)==0xf2a982caU,"negative variant bits changed");
    for(const int32_t value:{INT32_MIN,-1,0,INT32_MAX}){
        auto s=base;s.subscribers[0].quest_state->weapon_ilvl=value;s.subscribers[0].quest_state->weapon_variant_id=value;
        need(decode(encode(s),actual,e)&&actual==s,"int32 boundary tuple lost: "+e);
    }
    for(const auto& category:std::vector<std::string>{"","swords","curvedswords","Swords",std::string("custom-\xc3\xa4"),"quoted\"\\category",std::string(256,'x')}){
        auto s=base;s.subscribers[0].quest_state->weapon_category=category;
        need(decode(encode(s),actual,e)&&actual==s,"category was normalized/restricted: "+e);
    }
    for(unsigned mask=0;mask<7;++mask){
        auto bad=raw;const char* names[]={"weapon_ilvl","weapon_category","weapon_variant_id"};
        for(unsigned i=0;i<3;++i)if(!(mask&(1U<<i)))bad["subscribers"][0]["quest_state"].erase(names[i]);
        actual=base;need(!decode(bad,actual,e)&&!e.empty()&&actual==economy::State{},"partial equipment admitted or stale output retained");
    }
    auto old=raw;old["schema"]=11;need(!decode(old,actual,e)&&e.find("schema12")!=std::string::npos,"schema11 accepted new equipment");
    for(const char* field:{"weapon_ilvl","weapon_category","weapon_variant_id"})old["subscribers"][0]["quest_state"].erase(field);
    need(!decode(old,actual,e)&&e.find("schema12")!=std::string::npos,"legacy schema11 fabricated equipment defaults");
    for(const char* field:{"weapon_ilvl","weapon_variant_id"}){
        for(const Json& value:std::vector<Json>{nullptr,true,1.5,"12",Json::array(),Json::object(),int64_t(INT32_MIN)-1,uint64_t(INT32_MAX)+1,uint64_t(UINT32_MAX)}){
            auto bad=raw;bad["subscribers"][0]["quest_state"][field]=value;
            need(!decode(bad,actual,e)&&!e.empty(),std::string(field)+" malformed value admitted");
        }
    }
    for(const Json& value:std::vector<Json>{nullptr,true,1,1.5,Json::array(),Json::object(),std::string("swords\0foreign",14)}){
        auto bad=raw;bad["subscribers"][0]["quest_state"]["weapon_category"]=value;
        need(!decode(bad,actual,e)&&!e.empty(),"malformed category admitted");
    }
    auto bad=raw;bad["subscribers"][0]["quest_state"]["extra_weapon"]=1;need(!decode(bad,actual,e),"unknown equipment field admitted");
    auto toon=*base.subscribers[0].quest_state;toon.weapon_category=std::string(quest::weapon_category_max_bytes,'x');
    need(quest::valid(toon,e),"category byte ceiling incorrectly narrowed: "+e);
    toon.weapon_category.push_back('x');need(!quest::valid(toon,e)&&e.find("65536")!=std::string::npos,"category above ceiling accepted");
    toon.weapon_category=std::string("a\0b",3);need(!quest::valid(toon,e)&&e.find("NUL")!=std::string::npos,"native C-string suffix accepted");
    auto oversized=base;oversized.subscribers[0].quest_state->weapon_category=std::string(quest::weapon_category_max_bytes,'x');
    std::vector<uint8_t> bytes;need(economy::encode(oversized,bytes,e)&&bytes.size()>live::max_frame_payload&&decode(Json::parse(bytes),actual,e)&&actual==oversized,"large full frame roundtrip failed: "+e);
    for(int32_t id=64;id<84;++id){auto subscriber=oversized.subscribers[0];subscriber.serialized_id=id;oversized.subscribers.push_back(subscriber);}
    need(!economy::encode(oversized,bytes,e)&&e.find("1048576")!=std::string::npos,"logical reassembly bound bypassed");
    auto different=base;different.subscribers[0].quest_state->weapon_ilvl=13;
    need(economy::state_difference(different,base)=="subscriber[59].weapon_ilvl actual=13 required=12","item-level difference cause lost");
    different=base;different.subscribers[0].quest_state->weapon_category="swords";
    need(economy::state_difference(different,base)=="subscriber[59].weapon_category actual=\"swords\" required=\"wands\"","category difference cause lost");
    different=base;different.subscribers[0].quest_state->weapon_variant_id=1889777055;
    need(economy::state_difference(different,base)=="subscriber[59].weapon_variant_id actual=1889777055 required=-223771958","variant difference cause lost");
    std::cout<<"PASS "<<checks<<" actual equipment codec/validator/full-frame cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
