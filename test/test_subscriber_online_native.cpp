#include "live_host_state/day_rollover/economy/monetary.h"
#include "region_saved_identity.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
using namespace mt2mp::game::live_host_state;
struct FakeMemory {
    std::map<uintptr_t,unsigned char> bytes;
    bool copy(uintptr_t address,void* out,size_t size,std::string& e){auto* data=static_cast<unsigned char*>(out);for(size_t i=0;i<size;++i){auto found=bytes.find(address+i);if(found==bytes.end()){e="fixture missing native field";return false;}data[i]=found->second;}return true;}
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
} memory;
namespace building_mixed_identity::opened_file {struct CurrentWorld {void* world=nullptr;void* map=nullptr;uint64_t world_epoch=0,generation=0;};}
namespace native_open_runtime {building_mixed_identity::opened_file::CurrentWorld world;bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out=world;return true;}}
namespace sample {
using Owners=day::economy::Owners;
bool fail(std::string& e,const std::string& value){e=value;return false;}
template<class T>bool at(uintptr_t base,size_t offset,T& out,std::string& e){return memory.copy(base+offset,&out,sizeof out,e);}
bool ap(uintptr_t address,uintptr_t image,uintptr_t expected,std::string& e){uintptr_t value=0;return at(address,0,value,e)&&(value==image+expected||fail(e,"fixture wrong native addresspoint"));}
namespace subscriber_motion=mt2mp::game::subscriber_motion;
namespace region_saved_identity=mt2mp::game::region_saved_identity;
// Fake-memory motion dependency retained from the existing native lifecycle fixture.
namespace {
using Motion=subscriber_motion::Pose;
struct MotionOwner {uintptr_t map=0,root=0,toon=0,region=0,manager=0;};
bool motion_world(const Pin& p,MotionOwner& out,std::string& e){
    building_mixed_identity::opened_file::CurrentWorld w;
    if(!native_open_runtime::read_current_world(w,e))return false;
    if(reinterpret_cast<uintptr_t>(w.world)!=p.world||w.world_epoch!=p.binding.native_epoch||w.generation!=p.binding.native_generation)return fail(e,"Subscriber motion actual world binding differs");
    out.map=reinterpret_cast<uintptr_t>(w.map);out.root=out.map+0x1c0;return true;
}
bool motion_region(const Pin& p,const MotionOwner& owner,int32_t id,uintptr_t expected,uintptr_t& region,std::string& e){
    if(!region_saved_identity::resolve(p.image,owner.map,owner.root,id,expected,region,[](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},e))return false;
    uint8_t owned=0;if(!at(region,0x850,owned,e))return false;
    return owned==1||fail(e,"Subscriber motion target Region is not natively owned");
}
bool motion_manager(const Pin& p,uintptr_t region,uintptr_t& manager,std::vector<uintptr_t>& toons,std::string& e){
    uintptr_t data=0;int32_t count=-1,capacity=-1;uint8_t updating=1;
    if(!at(region,0x9c0,manager,e)||!ap(manager,p.image,0x16fde30,e)||!at(manager,0xc8,updating,e)||!at(manager,0x28,data,e)||!at(manager,0x30,count,e)||!at(manager,0x34,capacity,e))return false;
    if(updating||count<0||capacity<count||count>1000000||(count&&!data))return fail(e,"Subscriber motion ToonManager is updating or array bounds differ");
    toons.clear();std::set<uintptr_t> unique;
    for(int32_t i=0;i<count;++i){uintptr_t toon=0;if(!at(data,size_t(i)*8,toon,e))return false;if(!toon||!unique.insert(toon).second)return fail(e,"Subscriber motion ToonManager null/duplicate receiver");toons.push_back(toon);}
    return true;
}
bool motion_read(const Pin& p,uintptr_t subscriber,std::optional<Motion>& pose,std::string& e,MotionOwner* selected=nullptr){
    pose.reset();uintptr_t toon=0;if(!at(subscriber,0x80,toon,e))return false;
    if(!toon)return true;
    uintptr_t backlink=0;uint8_t logged=0;if(!ap(toon,p.image,0x1744e90,e)||!at(toon,0x940,backlink,e)||backlink!=subscriber||!at(toon,0x825,logged,e)||logged>1)return fail(e,e.empty()?"Subscriber motion reciprocal native Toon/login differs":e);
    if(!logged)return true;
    Motion value;MotionOwner owner;owner.toon=toon;uintptr_t shard=0,last=0,render=0,callback=0;
    if(!motion_world(p,owner,e)||!at(toon,0x170,shard,e)||!at(toon,0x178,owner.region,e)||!at(toon,0x9a8,last,e)||!at(owner.region,0x834,value.region,e))return false;
    if(shard!=owner.root||last!=owner.region)return fail(e,"Subscriber motion requires reciprocal registered root-shard Toon");
    uintptr_t region=0;if(!motion_region(p,owner,value.region,owner.region,region,e)||!at(toon,0x30,render,e)||!at(render,0x38,callback,e))return false;
    if(callback!=p.image+0x3e82a0)return fail(e,"Subscriber motion native Toon transform callback differs");
    if(!memory.copy(toon+0x100,value.orientation.data(),sizeof value.orientation,e)||!memory.copy(toon+0x110,value.position.data(),sizeof value.position,e)||!memory.copy(toon+0x11c,value.scale.data(),sizeof value.scale,e)||!subscriber_motion::valid(value,e))return false;
    std::vector<uintptr_t> registered;if(!motion_manager(p,owner.region,owner.manager,registered,e)||std::count(registered.begin(),registered.end(),toon)!=1)return fail(e,e.empty()?"Subscriber motion Toon absent from registered Region manager":e);
    pose=value;if(selected)*selected=owner;return true;
}
}

bool motion_target(const Pin&,const MotionOwner& owner,const Motion&,uintptr_t& region,std::string&){region=owner.region;return true;}
bool compare_owned_pointer(uintptr_t,uintptr_t,uintptr_t,std::string&);
#include "online_sample.inc"
}
namespace {
unsigned checks=0;
void check(bool value,const char* cause){if(!value)throw std::runtime_error(cause);++checks;}
constexpr uintptr_t image=0x140000000,map=0x100000,fixture_subscriber=0x200000,fixture_toon=0x300000,region_address=0x400000,fixture_manager=0x500000,fixture_rows=0x600000,root=map+0x1c0;
Pin fixture(){
    memory.bytes.clear();Pin p;p.image=image;p.world=0x700000;p.binding.native_epoch=7;p.binding.native_generation=9;
    native_open_runtime::world={reinterpret_cast<void*>(p.world),reinterpret_cast<void*>(map),7,9};
    memory.put(fixture_subscriber+0x80,fixture_toon);memory.put(fixture_toon,image+0x1744e90);memory.put(fixture_toon+0x940,fixture_subscriber);memory.put(fixture_toon+0x825,uint8_t(1));
    memory.put(fixture_toon+0x170,root);memory.put(fixture_toon+0x178,region_address);memory.put(fixture_toon+0x9a8,region_address);memory.put(fixture_toon+0x30,uintptr_t(0x710000));memory.put(uintptr_t(0x710038),image+0x3e82a0);
    memory.put(fixture_toon+0x100,std::array<float,4>{0,0,0,1});memory.put(fixture_toon+0x110,std::array<float,3>{12,0,20});memory.put(fixture_toon+0x11c,std::array<float,3>{1,1,1});
    memory.put(map,image+0x1742860);memory.put(root,image+0x174ad20);memory.put(root+0x48,int32_t(0));memory.put(map+0x230,image+0x16f04e0);memory.put(map+0x238,fixture_rows);memory.put(map+0x240,int32_t(1));memory.put(map+0x244,int32_t(1));
    memory.put(map+0x358,fixture_rows);memory.put(map+0x360,int32_t(1));memory.put(map+0x364,int32_t(1));memory.put(fixture_rows,region_address);
    memory.put(region_address,image+0x1787930);memory.put(region_address+0x70,root);memory.put(region_address+0x834,int32_t(67));memory.put(region_address+0x850,uint8_t(1));memory.put(region_address+0x9c0,fixture_manager);
    memory.put(fixture_manager,image+0x16fde30);memory.put(fixture_manager+0xc8,uint8_t(0));memory.put(fixture_manager+0x28,fixture_rows+0x100);memory.put(fixture_manager+0x30,int32_t(1));memory.put(fixture_manager+0x34,int32_t(2));memory.put(fixture_rows+0x100,fixture_toon);return p;
}
}

namespace {
constexpr uintptr_t sub_manager=0x800000,fixture_other=0x210000,off_sub=0x220000,other_toon=0x310000;
int calls_count=0,fault=0,cas_count=0,cas_fault=0,capacity_calls=0;bool empty=true;int fixture_capacity=20;
void array(uintptr_t owner,size_t base,std::initializer_list<uintptr_t> values){
    const uintptr_t data=owner+0x2000+base*16;memory.put(owner+base+8,data);memory.put(owner+base+0x10,int32_t(values.size()));memory.put(owner+base+0x14,int32_t(20));size_t i=0;for(auto value:values)memory.put(data+8*i++,value);
}
day::economy::Owners owner_fixture(bool online){
    auto p=fixture();calls_count=0;fault=0;cas_count=0;cas_fault=0;capacity_calls=0;empty=true;fixture_capacity=20;
    day::economy::Owners out;out.history.pin=p;out.subscriber_manager=sub_manager;out.subscribers={fixture_subscriber,fixture_other,off_sub};
    day::economy::Subscriber row;row.serialized_id=34;if(online)row.motion=sample::Motion{67,{0,0,0,1},{12,0,20},{1,1,1}};out.state.subscribers={row};
    memory.put(fixture_subscriber+0x20,int32_t(34));memory.put(fixture_toon+0x18,uint64_t(6115));memory.put(fixture_toon+0x824,uint8_t(1));memory.put(fixture_toon+0x825,uint8_t(online));
    memory.put(fixture_toon+0x368,uint8_t(online));memory.put(fixture_toon+0x9ed,uint8_t(0));memory.put(fixture_toon+0x9ec,uint8_t(0));memory.put(fixture_toon+0x6f0,uintptr_t(online?0x900000:0));memory.put(fixture_manager+0x18,region_address);
    memory.put(fixture_other+0x80,other_toon);memory.put(other_toon,image+0x1744e90);memory.put(other_toon+0x940,fixture_other);memory.put(other_toon+0x825,uint8_t(1));memory.put(off_sub+0x80,uintptr_t(0));
    array(fixture_manager,0x20,online?std::initializer_list<uintptr_t>{fixture_toon,other_toon}:std::initializer_list<uintptr_t>{other_toon});
    for(size_t base:{size_t(0x38),size_t(0x50),size_t(0x68)})array(fixture_manager,base,{});
    array(sub_manager,0x80,online?std::initializer_list<uintptr_t>{fixture_subscriber,fixture_other}:std::initializer_list<uintptr_t>{fixture_other});
    array(sub_manager,0x98,online?std::initializer_list<uintptr_t>{off_sub}:std::initializer_list<uintptr_t>{off_sub,fixture_subscriber});return out;
}
void logout(uintptr_t receiver){check(receiver==fixture_toon,"wrong original logout receiver");++calls_count;memory.put(fixture_toon+0x9ed,uint8_t(1));array(fixture_manager,0x38,{fixture_toon});if(fault==1)array(fixture_manager,0x38,{fixture_toon,other_toon});}
void notify(uintptr_t m,uintptr_t s){check(m==sub_manager&&s==fixture_subscriber,"wrong notify receiver");++calls_count;array(sub_manager,0x80,{fixture_other});array(sub_manager,0x98,{off_sub,fixture_subscriber});}
void drain(uintptr_t m){check(m==fixture_manager,"wrong original queue fixture_manager");++calls_count;array(fixture_manager,0x38,{});array(fixture_manager,0x20,{other_toon});memory.put(fixture_toon+0x825,uint8_t(0));memory.put(fixture_toon+0x9ed,uint8_t(0));memory.put(fixture_toon+0x6f0,uintptr_t(0));if(fault==2)memory.put(fixture_toon+0x18,uint64_t(999));if(fault==3)array(fixture_manager,0x20,{});}
void login(uintptr_t m,uintptr_t t){check(m==fixture_manager&&t==fixture_toon,"wrong original login receiver");++calls_count;uintptr_t last=0;std::string e;check(memory.copy(fixture_toon+0x9a8,&last,sizeof last,e)&&last==region_address,"original login did not receive initialized last Region");array(fixture_manager,0x20,{other_toon,fixture_toon});array(sub_manager,0x80,{fixture_other,fixture_subscriber});array(sub_manager,0x98,{off_sub});memory.put(fixture_toon+0x825,uint8_t(1));memory.put(fixture_toon+0x6f0,uintptr_t(0x900000));if(fault==4)memory.put(fixture_toon+0x6f0,uintptr_t(0));if(fault==5)memory.put(fixture_toon+0x9a8,uintptr_t(0));if(fault==6)memory.put(fixture_toon+0x170,uintptr_t(0));}
int32_t cap(uintptr_t){++capacity_calls;if(fault==7&&capacity_calls==1)memory.put(fixture_toon+0x18,uint64_t(999));return fixture_capacity;}bool no_login(uintptr_t){return empty;}
sample::OnlineCalls api(){return {logout,notify,drain,login,cap,no_login};}
bool move(const sample::Motion&,const sample::Motion& to,std::string&){memory.put(fixture_toon+0x110,to.position);memory.put(fixture_toon+0x100,to.orientation);memory.put(fixture_toon+0x11c,to.scale);return true;}
}
namespace sample {
bool compare_owned_pointer(uintptr_t address,uintptr_t expected,uintptr_t value,std::string& e){
    ++cas_count;check(address==fixture_toon+0x9a8&&expected==0&&value==region_address,"wrong loaded login CAS slot/values");
    if(cas_fault==1)memory.put(address,uintptr_t(0x1234));
    uintptr_t old=0;if(!memory.copy(address,&old,sizeof old,e))return false;
    if(old!=expected)return fail(e,"Day exact expected pointer owner differs");
    if(cas_fault==2)return fail(e,"Day pointer CAS SEH=3221225477");
    memory.put(address,value);
    if(cas_fault==3)memory.put(address,uintptr_t(0));
    if(cas_fault==4)memory.bytes.erase(address);
    return true;
}
}
struct LifecyclePort {
    bool store_lifecycle(const day::economy::Owners& before,size_t index,const std::optional<sample::Motion>& old,
                         const std::optional<sample::Motion>& target,std::string& e){return sample::online_apply(before,index,old,target,api(),move,e);}
};
day::economy::Receipt lifecycle_loop(const day::economy::Owners& expected,const day::economy::State& target){
    const auto& before=expected.state;LifecyclePort port;day::economy::Receipt receipt;
    auto stop=[&](const char* message){receipt.unknown=receipt.attempted;receipt.cause=std::string(message)+" original="+receipt.cause;};
#include "lifecycle_loop.inc"
    receipt.confirmed=true;return receipt;
}
int main(){try{
    check(reinterpret_cast<uintptr_t>(sample::online_calls(image).logout)==image+0x3e9920,"wrong pinned logout API");
    std::string error;sample::OnlineOwner held;
    auto before=owner_fixture(true);check(sample::online_apply(before,0,before.state.subscribers[0].motion,{},api(),move,error)&&calls_count==3,"exact native logout/notify/queue completion failed");
    before=owner_fixture(false);sample::Motion target{67,{0,0,0,1},{18,0,20},{1,1,1}};
    check(sample::online_apply(before,0,{},target,api(),move,error)&&calls_count==1,"native login followed by shared target pose failed");
    for(int mode=0;mode<10;++mode){before=owner_fixture(true);
        if(mode==0)memory.put(fixture_toon+0x824,uint8_t(0));if(mode==1)memory.put(fixture_toon+0x9ed,uint8_t(1));if(mode==2)memory.put(fixture_toon+0x9ec,uint8_t(1));if(mode==3)memory.put(fixture_toon+0x18,uint64_t(0));if(mode==4)memory.put(fixture_toon+0x6f0,uintptr_t(0));if(mode==5)array(fixture_manager,0x50,{other_toon});if(mode==6)array(sub_manager,0x98,{off_sub,fixture_other});if(mode==7)array(fixture_manager,0x20,{other_toon});if(mode==8)empty=false;if(mode==9)memory.put(fixture_toon+0x940,fixture_other);
        check(!sample::online_apply(before,0,before.state.subscribers[0].motion,{},api(),move,error)&&calls_count==0&&!error.empty(),"bad native lifecycle guard allowed an effect");
    }
    before=owner_fixture(false);fixture_capacity=1;check(!sample::online_prepare(before,0,target,api(),held,error)&&calls_count==0,"native login fixture_capacity ignored");
    for(int mode=1;mode<=4;++mode){const bool initial=mode!=4;before=owner_fixture(initial);fault=mode;
        const std::optional<sample::Motion> target_pose=initial?std::nullopt:std::optional<sample::Motion>{target};
        check(!sample::online_apply(before,0,before.state.subscribers[0].motion,target_pose,api(),move,error)&&calls_count>0&&!error.empty(),"partial native effect/UID/foreign membership/actor error accepted");
        if(mode==1)check(calls_count==1,"foreign queue was drained or Notify replayed after failed original readback");
    }
    before=owner_fixture(false);memory.put(fixture_toon+0x9a8,uintptr_t(0));
    check(sample::online_prepare(before,0,target,api(),held,error)&&held.initialize_last_region&&cas_count==0&&calls_count==0,"loaded offline preflight mutated or rejected null last Region");
    auto wanted=before.state;wanted.subscribers[0].motion=target;
    auto receipt=lifecycle_loop(before,wanted);
    check(receipt.confirmed&&receipt.attempted&&receipt.attempted_stores==1&&receipt.stores==1&&cas_count==1&&calls_count==1,"loaded offline first login did not execute CAS then original exactly once");
    check(!sample::online_apply(before,0,{},target,api(),move,error)&&cas_count==1&&calls_count==1,"stale offline receipt replayed login");
    for(int mode=0;mode<7;++mode){
        before=owner_fixture(false);memory.put(fixture_toon+0x9a8,uintptr_t(0));
        if(mode==0)memory.put(fixture_toon+0x9a8,fixture_other);
        if(mode==1)memory.put(fixture_toon+0x170,uintptr_t(0));
        if(mode==2)memory.put(fixture_toon+0x368,uint8_t(1));
        if(mode==3)array(fixture_manager,0x20,{other_toon,fixture_toon});
        if(mode==4)fixture_capacity=1;
        if(mode==5)memory.put(region_address+0x850,uint8_t(0));
        if(mode==6)memory.put(fixture_toon+0x940,fixture_other);
        error.clear();check(!sample::online_apply(before,0,{},target,api(),move,error)&&cas_count==0&&calls_count==0&&!error.empty(),"invalid loaded login owner admitted a CAS/callback");
    }
    before=owner_fixture(true);memory.put(fixture_toon+0x9a8,uintptr_t(0));
    check(!sample::online_apply(before,0,before.state.subscribers[0].motion,{},api(),move,error)&&cas_count==0&&calls_count==0,"online/logout null last Region was relaxed");
    before=owner_fixture(false);memory.put(fixture_toon+0x9a8,uintptr_t(0));fault=7;
    check(!sample::online_apply(before,0,{},target,api(),move,error)&&cas_count==0&&calls_count==0&&error.find("owner changed")!=std::string::npos,"fresh UID comparison lost before CAS");
    for(int mode=1;mode<=4;++mode){
        before=owner_fixture(false);memory.put(fixture_toon+0x9a8,uintptr_t(0));cas_fault=mode;wanted=before.state;wanted.subscribers[0].motion=target;
        receipt=lifecycle_loop(before,wanted);
        check(!receipt.confirmed&&receipt.attempted&&receipt.unknown&&receipt.attempted_stores==1&&receipt.stores==0&&cas_count==1&&calls_count==0&&!receipt.cause.empty(),"CAS/readback failure lost attempted Unknown or invoked login");
        if(mode==2)check(receipt.cause.find("SEH=3221225477")!=std::string::npos,"original CAS SEH cause lost");
        if(mode==4)check(receipt.cause.find("fixture missing native field")!=std::string::npos,"original readback cause lost");
    }
    for(int mode:{4,5,6}){
        before=owner_fixture(false);memory.put(fixture_toon+0x9a8,uintptr_t(0));fault=mode;wanted=before.state;wanted.subscribers[0].motion=target;
        receipt=lifecycle_loop(before,wanted);
        check(!receipt.confirmed&&receipt.unknown&&receipt.attempted_stores==1&&receipt.stores==0&&cas_count==1&&calls_count==1,"post-login failure accepted or lost partial effect");
    }
    std::cout<<"PASS "<<checks<<" actual lifecycle helper guards/call ordering/UID/actor/foreign partitions; native calls simulated; game_calls=0\n";
}catch(const std::exception& x){std::cerr<<"error: "<<x.what()<<'\n';return 1;}}
