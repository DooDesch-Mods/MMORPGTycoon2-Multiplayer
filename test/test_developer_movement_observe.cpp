#include "movement_observation.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
using namespace mt2mp::game;
namespace {
std::map<uintptr_t,unsigned char> fixture_bytes;
remove_diagnostics::LocalTarget bound;
developer_staff::Capture capture;
developer_staff::job_census::Census census;
std::string resolver_error;
int checks=0,resolves=0,passes=0;
bool change_owner=false,change_binding=false,change_uid=false;
constexpr uintptr_t image=0x10000000,map=0x20000000,actor=0x30000000,building=0x40000000;
constexpr uintptr_t selection=0x50000000,fixture_region=0x60000000,array=0x61000000;
template<class T>void put(uintptr_t p,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof(T);++n)fixture_bytes[p+n]=data[n];}
bool read(uintptr_t p,void* target,size_t n,std::string& error){
    if(p==actor+0x170&&++passes==2&&change_owner)put(actor+0x488,uintptr_t(0x88888888));
    auto* out=static_cast<unsigned char*>(target);
    for(size_t i=0;i<n;++i){const auto found=fixture_bytes.find(p+i);if(found==fixture_bytes.end()){error="original movement read cause";return false;}out[i]=found->second;}
    return true;
}
void need(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
void reset(){fixture_bytes.clear();capture={};census={};resolver_error.clear();resolves=passes=0;change_owner=change_binding=change_uid=false;
    capture.image=image;capture.pin.origin.map=reinterpret_cast<void*>(map);capture.pin.origin.uuid="native-origin";
    capture.pin.origin.world_epoch=4;capture.pin.origin.generation=7;
    for(int32_t i=0;i<50;++i){const auto receiver=actor+uintptr_t(i)*0x1000;
        for(size_t n=0;n<0x500;++n)fixture_bytes[receiver+n]=0;
        capture.state.rows.push_back({100+i,i});capture.local.developers.push_back(receiver);
        developer_staff::job_census::Developer dev;dev.pointer=receiver;dev.id=100+i;census.developers.push_back(dev);
        put(receiver+0x100,std::array<float,4>{0,0,0,1});put(receiver+0x11c,std::array<float,3>{1,1,1});
    }
    put(image+0x13df630,selection);put(selection,image+uintptr_t(0x17205d0));
    bound={selection,building,42,4,7,"common-snapshot","action:building","SmallInn","base",true,false,{}};
}
void root_region(){const uintptr_t root=map+0x1c0;
    put(map,image+uintptr_t(0x1742860));put(root,image+uintptr_t(0x174ad20));put(root+0x48,int32_t(0));
    put(map+0x230,image+uintptr_t(0x16f04e0));put(map+0x238,array);put(map+0x240,int32_t(1));put(map+0x244,int32_t(1));
    put(map+0x358,uintptr_t(0));put(map+0x360,int32_t(0));put(map+0x364,int32_t(0));put(array,fixture_region);
    put(fixture_region,image+uintptr_t(0x1787930));put(fixture_region+0x70,root);put(fixture_region+0x834,int32_t(33));
    put(actor+0x170,root);put(actor+0x178,fixture_region);
}
}
namespace mt2mp::game::remove_diagnostics {
bool resolve_local_target(uintptr_t,uintptr_t,LocalTarget& out,std::string& error){
    ++resolves;if(!resolver_error.empty()){error=resolver_error;return false;}out=bound;
    if(change_binding&&resolves==2)out.record_key="action:changed";
    if(change_uid&&resolves==2)++out.uid;return true;
}
}
int main(){try{namespace j=developer_staff::job_census;nlohmann::json out;std::string error;
    auto run=[&]{passes=resolves=0;return j::observe_movement(read,capture,census,out,error);};
    reset();need(run()&&out.at("actors").size()==50&&!out.at("movement_graph_identity_proven").get<bool>(),"fixed50 null owners");
    need(!out.at("actors")[0].at("current_building").at("present").get<bool>()&&out.at("actors")[0].at("region_matches_job").is_null(),"null owner is explicit");
    reset();root_region();put(actor+0x338,building);put(actor+0x348,building);
    need(run()&&out.at("actors")[0].at("region").at("saved_id")==33&&out.at("actors")[0].at("shard_is_root")==true&&
         out.at("actors")[0].at("destination_building").at("binding_snapshot_uuid")=="common-snapshot","actual saved Region and distinct binding snapshot");
    need(out.dump().find(std::to_string(building))==std::string::npos&&out.dump().find(std::to_string(fixture_region))==std::string::npos,"no local pointer serialization");
    reset();root_region();j::Job job;job.pointer=0x70000000;job.region=fixture_region;census.jobs.push_back(job);census.developers[0].job=job.pointer;census.developers[0].job_index=0;
    need(run()&&out.at("actors")[0].at("region_matches_job")==true,"actual assigned Job Region");
    census.developers[0].job_index=1;need(!run()&&error.find("absent from census")!=std::string::npos,"invalid Job index");
    reset();capture.state.rows.pop_back();need(!run()&&!out.at("observed").get<bool>(),"short fixed50 rejected");
    reset();census.developers[0].pointer+=8;need(!run()&&error.find("receiver/ID/slot")!=std::string::npos,"wrong local actor");
    reset();capture.state.rows[1].id=100;census.developers[1].id=100;need(!run(),"duplicate actor ID");
    reset();fixture_bytes.erase(actor+0x269);need(!run()&&error=="original movement read cause","original read cause retained");
    reset();put(actor+0x269,uint8_t(2));need(!run()&&error.find("not Boolean")!=std::string::npos,"nonboolean movement flag");
    reset();put(actor+0x100,std::numeric_limits<float>::infinity());need(!run()&&error.find("orientation is not finite")!=std::string::npos,"invalid orientation");
    reset();put(actor+0x230,std::numeric_limits<float>::quiet_NaN());need(!run()&&error.find("destination is not finite")!=std::string::npos,"invalid movement destination");
    reset();root_region();put(fixture_region+0x70,uintptr_t(0));need(!run()&&error.find("saved region")!=std::string::npos,"wrong Region ownership");
    reset();put(actor+0x338,building);resolver_error="original selected Building binding cause";need(!run()&&error==resolver_error,"original resolver cause retained");
    reset();put(actor+0x338,building);bound.world_generation=8;need(!run()&&error.find("Origin/receiver")!=std::string::npos,"stale Building generation");
    reset();put(actor+0x338,building);bound.snapshot_id.clear();need(!run(),"empty shared binding rejected");
    reset();put(actor+0x338,building);bound.object+=8;need(!run(),"wrong Building receiver");
    reset();put(actor+0x338,building);change_binding=true;need(!run()&&error.find("changed between reads")!=std::string::npos,"changed key rejected");
    reset();put(actor+0x488,uintptr_t(0x77777777));change_owner=true;need(!run()&&error.find("local owners changed")!=std::string::npos,"same presence different local pointer rejected");
    reset();put(actor+0x338,building);change_uid=true;need(!run()&&error.find("local owners changed")!=std::string::npos,"same key changed local UID rejected");
    std::cout<<"PASS "<<checks<<" movement observation checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
