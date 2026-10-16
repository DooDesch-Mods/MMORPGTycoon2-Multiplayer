#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace staff=mt2mp::game::developer_staff;
namespace bug=mt2mp::game::bug_authority;
namespace {
size_t checks=0;
void need(bool ok,const std::string& e){++checks;if(!ok)throw std::runtime_error(e);}
economy::State fixture(){
    economy::State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";
    f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;
    staff::PoolState p;staff::construction::State c;
    for(int32_t i=0;i<50;++i){staff::PoolRow r;r.id=i;p.rows.push_back(r);p.indoor.push_back(i);c.actors.push_back({i,-1,{0,0,0,1},{1,1,1}});}
    bug::WorkState w;bug::JobRow j;j.bug.parent_key="action:"+f.snapshot;j.bug.birth={bug::BirthKind::Publication,f.snapshot,2,0};j.birth={bug::BirthKind::Publication,f.snapshot,3,0};w.jobs.push_back(j);
    c.work=w;p.construction=c;f.developers=p;f.bugs=bug::State{};s.day.history={{1,live::day::record(f.values)}};return s;
}
bool decode(const nlohmann::json& j,economy::State& out,std::string& e){const auto b=j.dump();return economy::decode(reinterpret_cast<const uint8_t*>(b.data()),b.size(),out,e);}
}
int main(){try{
    auto s=fixture();std::string e;std::vector<uint8_t> bytes;economy::State actual;
    need(economy::encode(s,bytes,e)&&economy::decode(bytes.data(),bytes.size(),actual,e)&&actual==s,"full Type7 frame roundtrip: "+e);
    auto raw=nlohmann::json::parse(bytes);need(raw["day"]["frame"]["schema"]==12,"schema12 absent");
    auto missing=raw;missing["day"]["frame"]["bugs"]=nullptr;need(!decode(missing,actual,e),"Work bypassed complete Bug preflight state");
    auto bad=raw;bad["day"]["frame"]["developers"]["construction"].erase("job_fixed");need(!decode(bad,actual,e),"partial Work accepted");
    for(int schema:{10,11}){auto old=raw;old["day"]["frame"]["schema"]=schema;if(schema==10)old["day"]["frame"].erase("bugs");
        need(!decode(old,actual,e),"legacy schema admitted Work");
        for(const auto* key:{"bug_jobs","bug_parents","job_created","job_fixed"})old["day"]["frame"]["developers"]["construction"].erase(key);
        need(decode(old,actual,e)&&!actual.day.frame.developers->construction->work,"legacy Work absence lost: "+e);}
    auto old=raw;old["day"]["frame"]["schema"]=9;old["day"]["frame"].erase("developers");old["day"]["frame"].erase("bugs");need(decode(old,actual,e)&&!actual.day.frame.developers&&!actual.day.frame.bugs,"schema9 decode changed");
    for(const size_t field:{3u,7u}){bad=raw;bad["day"]["frame"]["developers"]["construction"]["bug_jobs"][0][field]="22222222-2222-4222-8222-222222222222";need(!decode(bad,actual,e),"foreign publication source accepted");}
    for(const size_t field:{4u,8u}){bad=raw;bad["day"]["frame"]["developers"]["construction"]["bug_jobs"][0][field]=4;need(!decode(bad,actual,e),"future publication accepted");}
    s.day.frame.developers->construction->work=bug::WorkState{};need(economy::encode(s,bytes,e)&&economy::decode(bytes.data(),bytes.size(),actual,e)&&actual==s,"empty authoritative Work lost");
    auto before=fixture().day.frame;auto after=before;after.developers->construction->work->created.value.today=1;
    live::Context c{41,2,1,3,mt2mp::action_session::Role::Client,before.snapshot};
    need(live::admit(1,c,7,9,&before,after,0,0,false,e)==live::Admission::Rejected,"same-revision counter conflict admitted");
    std::cout<<"PASS "<<checks<<" actual Type7 full-frame cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
