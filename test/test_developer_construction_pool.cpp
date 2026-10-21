#include "developer_staff/frame_apply.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace {unsigned checks=0;void need(bool v,const std::string& e){++checks;if(!v)throw std::runtime_error(e);}
staff::PoolState baseline(){staff::PoolState s;s.count=1;for(int32_t i=0;i<50;++i){staff::PoolRow r;r.id=49-i;r.employed=r.active=i==0;s.rows.push_back(r);s.indoor.push_back(r.id);}return s;}
staff::PoolState construction(){auto s=baseline();staff::construction::State c;for(const auto& r:s.rows)c.actors.push_back({r.id,-1,{0,0,0,1},{1,1,1}});c.jobs.push_back({"action:11111111-1111-4111-8111-111111111111",true,false,-1});s.construction=c;return s;}
}
int main(){try{std::string e;staff::PoolState out;auto old=baseline();const auto legacy=staff::encode(old);
need(legacy.size()==4&&!legacy.contains("construction"),"legacy shape changed");need(staff::decode(legacy,out,e)&&out==old,"legacy roundtrip "+e);
auto s=construction();need(staff::valid(s,e),"unassigned pending "+e);const auto wire=staff::encode(s);need(staff::decode(wire,out,e)&&out==s,"construction roundtrip "+e);
auto assigned=s;assigned.rows[0].job_link=assigned.rows[0].on_map=true;assigned.indoor.erase(assigned.indoor.begin());assigned.outdoor={49};assigned.construction->jobs[0].developer_id=49;
need(staff::valid(assigned,e),"assigned inactive Building rejected "+e);assigned.construction->jobs[0].active=true;need(staff::valid(assigned,e),"transient active Building rejected "+e);
for(const auto& saved_key:{std::string(64,'a')+":world:building:12",std::string(64,'b')+":world:building:inline:region{60}/buildingManager/building[0]"}){
    auto loaded=assigned;loaded.construction->jobs[0].building_key=saved_key;
    need(staff::valid(loaded,e)&&staff::decode(staff::encode(loaded),out,e)&&out==loaded,"loaded partial construction snapshot key/progress roundtrip "+e);
    auto wrong=loaded;wrong.construction->jobs[0].building_key=saved_key+"foreign";need(!staff::valid(wrong,e),"malformed snapshot construction key admitted");
}
auto completed=s;completed.construction->jobs.clear();need(staff::valid(completed,e)&&staff::decode(staff::encode(completed),out,e)&&out==completed,"explicit empty pending set rejected "+e);
auto restored=s;restored.construction->jobs[0].present=false;need(staff::valid(restored,e)&&staff::decode(staff::encode(restored),out,e)&&out==restored,"restored inactive row rejected "+e);
auto invalid=restored;invalid.construction->jobs[0].developer_id=49;need(!staff::valid(invalid,e),"absent assigned row accepted");
invalid=s;invalid.construction->jobs[0]={s.construction->jobs[0].building_key,false,true,-1};need(!staff::valid(invalid,e),"completed lifetime row accepted on pending-only wire");
invalid=s;invalid.construction->jobs[0].developer_id=49;need(!staff::valid(invalid,e),"nonreciprocal assigned accepted");
invalid=assigned;invalid.construction->jobs.push_back({"action:22222222-2222-4222-8222-222222222222",true,false,49});need(!staff::valid(invalid,e),"double assignment accepted");
invalid=s;invalid.construction->jobs.push_back(invalid.construction->jobs[0]);need(!staff::valid(invalid,e),"duplicate key accepted");
auto many=s;for(int i=1;i<=64;++i){auto key=s.construction->jobs[0].building_key;key.replace(key.size()-3,3,std::to_string(200+i));many.construction->jobs.push_back({key,true,false,-1});}need(staff::valid(many,e)&&staff::decode(staff::encode(many),out,e)&&out==many,"unproven64 lifetime cap retained: "+e);
invalid=s;invalid.construction->actors[0].id=0;need(!staff::valid(invalid,e),"slot confused with ID");
invalid=s;invalid.construction->actors[0].region_id=-2;need(!staff::valid(invalid,e),"invalid Region accepted");
invalid=s;invalid.construction->actors[0].orientation={0,0,0,0};need(!staff::valid(invalid,e),"zero quaternion accepted");
invalid=s;invalid.construction->actors[0].orientation[0]=std::numeric_limits<float>::infinity();need(!staff::valid(invalid,e),"infinite orientation accepted");
invalid=s;invalid.construction->actors[0].scale[1]=0;need(!staff::valid(invalid,e),"zero scale accepted");
invalid=s;invalid.construction->jobs[0].building_key="saved:1";need(!staff::valid(invalid,e),"unowned key accepted");
for(int kind=0;kind<7;++kind){auto bad=wire;if(kind==0)bad["construction"]=nullptr;if(kind==1)bad["construction"]["other"]=1;if(kind==2)bad["construction"]["jobs"][0][1]=1;if(kind==3)bad["construction"]["actors"][0][0]=49.0;if(kind==4)bad["construction"]["actors"][0].push_back(1);if(kind==5)bad["construction"]["jobs"][0].push_back(1);if(kind==6)bad["construction"]["actors"].erase(0);need(!staff::decode(bad,out,e)&&out==staff::PoolState{},"malformed optional shape accepted");}
std::vector<staff::FieldChange> changes;need(!staff::frame_plan(s,s,changes,e)&&e.find("bound native")!=std::string::npos,"legacy scalar route accepted construction");
std::cout<<"PASS "<<checks<<" construction portable checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
