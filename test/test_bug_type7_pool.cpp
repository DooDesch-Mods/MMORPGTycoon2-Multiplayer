#include "developer_staff/shared_state.h"
#include "bug_authority/job_codec.h"
#include <iostream>
namespace staff=mt2mp::game::developer_staff;
namespace bug=mt2mp::game::bug_authority;
using Json=nlohmann::json;
namespace {
size_t checks=0;
void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
const std::string id="11111111-1111-4111-8111-111111111111",sha(64,'a');
staff::PoolState pool(){
    staff::PoolState p;p.count=1;staff::construction::State c;
    for(int32_t i=0;i<50;++i){staff::PoolRow row;row.id=i;row.employed=row.active=i==0;p.rows.push_back(row);p.indoor.push_back(i);c.actors.push_back({i,-1,{0,0,0,1},{1,1,1}});}
    c.work=bug::WorkState{};p.construction=c;return p;
}
bug::JobRow job(){bug::JobRow j;j.bug.parent_key="action:"+id;j.bug.birth={bug::BirthKind::Action,id,0,0};j.birth={bug::BirthKind::Snapshot,sha,0,12};j.region_id=67;j.position={8000,70,8500};return j;}
void assign(staff::PoolState& p){p.rows[0].job_link=p.rows[0].on_map=true;p.indoor.erase(p.indoor.begin());p.outdoor.push_back(0);p.construction->work->jobs[0].developer_id=0;}
}
int main(){try{
    std::string e;auto p=pool();staff::PoolState actual;
    need(staff::decode(staff::encode(p),actual,e)&&actual==p,"explicit empty Work lost: "+e);
    p.construction->work.reset();need(staff::decode(staff::encode(p),actual,e)&&actual==p,"legacy Construction changed");
    p=pool();p.construction->work->jobs.push_back(job());assign(p);
    bug::ParentVisual building;building.key="action:"+id;building.bandwidth=.5f;building.user_set=true;p.construction->work->parents.push_back(building);
    bug::ParentVisual npc;npc.kind=bug::ParentKind::Npc;npc.key=sha+":world:npc:34";npc.tint_override=true;npc.tint={1,.5f,0,1};p.construction->work->parents.push_back(npc);
    p.construction->work->created.value={std::vector<int32_t>(10,1),2,12};p.construction->work->created.highest=2;p.construction->work->created.day=7;
    need(staff::decode(staff::encode(p),actual,e)&&actual==p,"assigned Type7 full pool roundtrip: "+e);
    const auto good=staff::encode(p);
    auto reject=[&](Json j){actual=p;need(!staff::decode(j,actual,e)&&actual==staff::PoolState{}&&!e.empty(),"malformed Work accepted or partial output: "+e);};
    for(const auto* name:{"bug_jobs","bug_parents","job_created","job_fixed"}){auto raw=good;raw["construction"].erase(name);reject(raw);}
    for(const size_t field:{0u,2u,4u,5u,6u,8u,9u,10u,12u,13u}){auto raw=good;raw["construction"]["bug_jobs"][0][field]=1.5;reject(raw);}
    for(const auto& [field,value]:std::vector<std::pair<size_t,Json>>{{0,2},{2,3},{4,-1},{6,3},{9,0},{10,1},{11,1},{11,false},{12,50},{13,-2},{14,"8000"}}){auto raw=good;raw["construction"]["bug_jobs"][0][field]=value;reject(raw);}
    auto raw=good;raw["construction"]["bug_jobs"][0].push_back(0);reject(raw);
    raw=good;raw["construction"]["bug_jobs"][0][9]=uint64_t(INT32_MAX)+1;reject(raw);
    raw=good;raw["construction"]["bug_jobs"].push_back(raw["construction"]["bug_jobs"][0]);reject(raw);
    raw=good;raw["construction"]["job_created"].push_back(0);reject(raw);
    raw=good;raw["construction"]["job_fixed"]=Json::array({0,0,0});reject(raw);
    for(size_t field=0;field<4;++field){raw=good;raw["construction"]["job_fixed"][field]=-1;reject(raw);}
    raw=good;raw["construction"]["job_fixed"]=Json::array({0,0,0,0,-1});reject(raw);
    for(const auto& [index,field,value]:std::vector<std::tuple<size_t,size_t,Json>>{{0,2,3},{0,5,true},{0,6,.1},{1,3,.1},{1,4,true},{1,5,1},{1,9,"1"}}){raw=good;raw["construction"]["bug_parents"][index][field]=value;reject(raw);}
    raw=good;raw["construction"]["bug_parents"].push_back(raw["construction"]["bug_parents"][0]);reject(raw);
    raw=good;raw["construction"]["bug_parents"][0][3]=-.1;reject(raw);
    raw=good;raw["rows"][0][5]=false;reject(raw);
    raw=good;raw["construction"]["jobs"].push_back(Json::array({"action:"+id,true,false,0}));reject(raw);
    raw=good;raw["construction"]["bug_jobs"][0][10]=0;raw["construction"]["bug_jobs"][0][11]=false;
    need(staff::decode(raw,actual,e)&&actual.construction->work->jobs[0].developer_id==0,"assigned aborted Job rejected: "+e);
    raw["construction"]["bug_jobs"][0][12]=-1;reject(raw);
    auto absent=p;auto& aborted=absent.construction->work->jobs[0];aborted.type=0;aborted.valid=false;aborted.bug={};
    need(staff::decode(staff::encode(absent),actual,e)&&actual==absent,"assigned Snapshot aborted Job lost exact absent provenance: "+e);
    raw=staff::encode(absent);raw["construction"]["bug_jobs"][0][3]=sha;reject(raw);
    raw=staff::encode(absent);raw["construction"]["bug_jobs"][0][10]=7;raw["construction"]["bug_jobs"][0][11]=true;reject(raw);
    auto conflict=pool();conflict.construction->work->jobs={job(),job()};conflict.construction->work->jobs[1].birth.occurrence=13;
    need(staff::valid(conflict,e),"same retained Bug provenance across distinct Jobs rejected: "+e);
    conflict.construction->work->jobs[1].bug.parent_key=sha+":world:building:34";
    need(!staff::valid(conflict,e)&&e.find("conflicting parents")!=std::string::npos,"global Bug birth aliases different parents");
    auto unassigned=pool();unassigned.construction->work->jobs.push_back(job());need(staff::valid(unassigned,e),"unassigned valid7 rejected: "+e);
    unassigned.construction->work->jobs[0].bug.birth={bug::BirthKind::Snapshot,sha,0,0};need(staff::valid(unassigned,e),"Bug ordinal0 confused with serialized Job linkId");
    unassigned.construction->work->jobs[0].birth={bug::BirthKind::Publication,id,7,0};need(staff::valid(unassigned,e),"Publication occurrence0 invalid");
    std::cout<<"PASS "<<checks<<" actual Type7 pool/codec cases\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
