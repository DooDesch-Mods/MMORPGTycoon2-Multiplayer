#include "intervention_records.h"
#include "native_open_runtime.h"
#include "root_saved_building.h"
#include "npc_quest_target_runtime.h"
#include "live_host_state/memory_query_timing.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <tuple>
namespace mt2mp::game::intervention_records {
namespace {
namespace c=subscriber_lifecycle;
using World=building_mixed_identity::opened_file::CurrentWorld;
struct Memory:c::Memory {bool read(uintptr_t address,void* out,size_t bytes,std::string& error)override{return root_saved_building::bytes(reinterpret_cast<void*>(address),0,out,bytes,error);}};
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>bool at(Memory& memory,uintptr_t object,size_t offset,T& out,std::string& error){if(!object||object>UINTPTR_MAX-offset)return fail(error,"Intervention original record native address extent invalid");return memory.read(object+offset,&out,sizeof out,error);}
uint32_t lookup(uintptr_t image,uint64_t uid,uintptr_t& found)noexcept{__try{found=reinterpret_cast<uintptr_t(*)(uint64_t)>(image+0x47ce20)(uid);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool same(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.uuid==b.uuid&&a.sha256==b.sha256;}
struct Pin {uintptr_t image=0,manager=0,data=0,object=0,toon=0,brain=0;int32_t count=0,capacity=0,id=-1;uint64_t toon_uid=0;bool operator==(const Pin&)const=default;};
bool current(Memory& memory,const World& world,int32_t selected,Pin& out,std::string& error){out={};Pin next;next.image=uintptr_t(GetModuleHandleW(nullptr));uintptr_t ap=0;
    if(!at(memory,next.image,0x13df690,next.manager,error)||!at(memory,next.manager,0,ap,error)||ap!=next.image+0x1725f40||!at(memory,next.manager,0x40,next.data,error)||!at(memory,next.manager,0x48,next.count,error)||!at(memory,next.manager,0x4c,next.capacity,error)){if(error.empty())error="Intervention Subscriber actual manager class/header differs";return false;}
    if(selected<0||next.count<0||next.count>1000000||next.capacity<next.count||(next.count&&(!next.data||next.data>UINTPTR_MAX-size_t(next.count)*8)))return fail(error,"Intervention Subscriber current manager span invalid");
    std::vector<uintptr_t> objects(size_t(next.count),uintptr_t{});if(next.count&&!memory.read(next.data,objects.data(),objects.size()*8,error))return false;
    std::set<int32_t> ids;for(const auto object:objects){int32_t id=-1;if(!at(memory,object,0,ap,error)||ap!=next.image+0x16e75f0||!at(memory,object,0x20,id,error)||id<0||!ids.insert(id).second){if(error.empty())error="Intervention Subscriber actual manager member class/ID duplicates or differs";return false;}if(id==selected){next.id=id;next.object=object;}}
    if(!next.object)return fail(error,"Intervention Subscriber target ID is absent from actual manager");
    uintptr_t reverse=0,found=0;if(!at(memory,next.object,0x80,next.toon,error)||!at(memory,next.object,0x88,next.brain,error)||!at(memory,next.toon,0,ap,error)||ap!=next.image+0x1744e90||!at(memory,next.toon,0x18,next.toon_uid,error)||!next.toon_uid||!at(memory,next.toon,0x940,reverse,error)||reverse!=next.object||!at(memory,next.brain,0,ap,error)||ap!=next.image+0x16fdd60||!at(memory,next.brain,0x28,reverse,error)||reverse!=next.object){if(error.empty())error="Intervention Subscriber exact Toon/Brain reciprocal ownership differs";return false;}
    const auto exception=lookup(next.image,next.toon_uid,found);if(exception||found!=next.toon)return fail(error,"Intervention Subscriber current full Toon UID lookup differs SEH="+std::to_string(exception));
    World checked;if(!native_open_runtime::read_current_world(checked,error)||!same(world,checked))return fail(error,error.empty()?"Intervention original record current NativeOpen world changed":error);
    out=next;error.clear();return true;
}
bool code(Memory& memory,uintptr_t image,std::string& error,bool include_subscriber=true){struct Body{uintptr_t rva;std::array<unsigned char,16> prefix;};const Body bodies[]{
    {0x9667c0,{0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0x89,0xcb,0xe8,0xb1,0x66,0x00,0x00,0x48}},
    {0x9676a0,{0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0x89,0xce,0xe8,0x61,0xed,0xff,0xff,0x48}},
    {0x89e580,{0x53,0x48,0x83,0xec,0x20,0x8b,0x05,0x25,0xe9,0x19,0x01,0x8d,0x50,0x01,0x89,0x15}},
    {0x89e5f0,{0x56,0x53,0x48,0x83,0xec,0x28,0x48,0x8b,0x31,0x48,0x85,0xf6,0x0f,0x84,0x7e,0x00}},
    {0xeb85f0,{0x53,0x48,0x83,0xec,0x30,0x48,0x8b,0x01,0x48,0x8b,0x40,0x30,0x49,0x89,0xd0,0x48}},
    {0xeb8650,{0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x58,0x48,0x8d,0x2d,0x5d}}
    };for(size_t i=0;i<std::size(bodies);++i){if(!include_subscriber&&i>=4)break;const auto& body=bodies[i];std::array<unsigned char,16> bytes{};if(!memory.read(image+body.rva,bytes.data(),bytes.size(),error))return false;if(bytes!=body.prefix)return fail(error,"Intervention original record capture helper differs RVA="+std::to_string(body.rva));}return true;}
bool same_array(const character_definition_identity::Array& a,const character_definition_identity::Array& b){return std::tie(a.object,a.data,a.count,a.capacity,a.members)==std::tie(b.object,b.data,b.count,b.capacity,b.members);}
bool same_definitions(const character_definition_identity::Census& a,const character_definition_identity::Census& b){
    if(a.state!=b.state||a.manager!=b.manager||a.definitions.size()!=b.definitions.size()||a.collections.size()!=b.collections.size())return false;
    for(size_t n=0;n<a.arrays.size();++n)if(!same_array(a.arrays[n],b.arrays[n]))return false;
    for(size_t n=0;n<a.definitions.size();++n){const auto& x=a.definitions[n];const auto& y=b.definitions[n];
        if(std::tie(x.key,x.kind,x.id,x.variant_slot,x.object,x.parent,x.owning_collection,x.variants,x.record,x.uid,x.parent_uid,x.deleted,x.edited,x.released)!=std::tie(y.key,y.kind,y.id,y.variant_slot,y.object,y.parent,y.owning_collection,y.variants,y.record,y.uid,y.parent_uid,y.deleted,y.edited,y.released))return false;}
    for(size_t n=0;n<a.collections.size();++n){const auto& x=a.collections[n];const auto& y=b.collections[n];if(x.object!=y.object||x.owner!=y.owner||x.ids!=y.ids)return false;for(size_t k=0;k<x.arrays.size();++k)if(!same_array(x.arrays[k],y.arrays[k]))return false;}
    return true;
}
struct Borrow {Memory& memory;const External& external;Subscriber& output;World world;Pin pin;};
bool inspect(void* user,const void* record,const void* context,std::string& error){auto& value=*static_cast<Borrow*>(user);auto& result=value.output.record;
    if(!c::read_graph(value.memory,uintptr_t(record),uintptr_t(context),{65536,262144,4*1024*1024,65536},result.original,error))return false;
    return duplicate_records::summarize(result.original,value.pin.object,value.external,result.portable,error);
}
bool body(void* user,std::string& error){auto& value=*static_cast<Borrow*>(user);Pin before;if(!current(value.memory,value.world,value.pin.id,before,error)||before!=value.pin)return fail(error,error.empty()?"Intervention Subscriber original owner changed before record capture":error);
    const auto image=value.pin.image;const c::CaptureApi api{reinterpret_cast<c::Construct>(image+0x9667c0),reinterpret_cast<c::Construct>(image+0x9676a0),reinterpret_cast<c::Construct>(image+0x89e580),reinterpret_cast<c::Construct>(image+0x89e5f0),reinterpret_cast<c::PrepareSave>(image+0xeb85f0),reinterpret_cast<c::SaveRecord>(image+0xeb8650)};
    return c::capture_original(api,reinterpret_cast<const void*>(value.pin.object),&inspect,&value,value.output.record.cleanup,error);
}
struct ZoneBorrow {Memory& memory;const External& external;uintptr_t object; npc_probe::character_actor_records::Result& result;};
bool inspect_zone(void* user,const void* record,const void* context,std::string& error){auto& value=*static_cast<ZoneBorrow*>(user);
    return c::read_graph(value.memory,uintptr_t(record),uintptr_t(context),{65536,262144,4*1024*1024,65536},value.result.original,error)&&duplicate_records::summarize(value.result.original,value.object,value.external,value.result.portable,error);
}
bool zone_pin(Memory& memory,const World& required,uintptr_t object,uint64_t uid,const std::string& key,uintptr_t& shard,uintptr_t& region,std::string& error){
    const auto image=uintptr_t(GetModuleHandleW(nullptr));uintptr_t ap=0,found=0;uint64_t actual_uid=0;std::string canonical;bool dungeon=false;
    if(!object||!uid||!at(memory,object,0,ap,error)||ap!=image+0x1745ce0||!at(memory,object,0x18,actual_uid,error)||actual_uid!=uid||!at(memory,object,0x170,shard,error)||!at(memory,object,0x178,region,error))return fail(error,error.empty()?"Intervention Zone original class/full UID/scene differs":error);
    const auto exception=lookup(image,uid,found);if(exception||found!=object)return fail(error,"Intervention Zone original full UID lookup differs SEH="+std::to_string(exception));
    if(!npc_quest_target_runtime::bind_current_destination(object,canonical,dungeon,error)||canonical!=key)return fail(error,error.empty()?"Intervention Zone current canonical owner differs":error);
    World actual;if(!native_open_runtime::read_current_world(actual,error)||!same(required,actual))return fail(error,error.empty()?"Intervention Zone actual NativeOpen changed":error);return true;
}
}
bool subscriber(const action_session::Context& context,const action_session::Binding& binding,int32_t id,const External& external,Subscriber& out,std::string& error){
    out={};if(!external)return fail(error,"Intervention Subscriber original capture requires canonical external owners");
    character_definition_identity::Census definitions;character_membership_reader::Users users;if(!npc_probe::character_actor_records::read_current(context,binding,definitions,users,error))return false;
    World world;if(!native_open_runtime::read_current_world(world,error))return false;Memory memory;Pin pin;if(!current(memory,world,id,pin,error)||!code(memory,pin.image,error))return false;
    Borrow value{memory,external,out,world,pin};
    {live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!body(&value,error))return false;}
    Pin final_pin;if(!current(memory,world,id,final_pin,error)||final_pin!=pin)return fail(error,error.empty()?"Intervention original Subscriber record changed actual manager/ID/Toon/Brain":error);
    character_definition_identity::Census final_definitions;character_membership_reader::Users final_users;if(!npc_probe::character_actor_records::read_current(context,binding,final_definitions,final_users,error))return false;
    if(!same_definitions(final_definitions,definitions)||final_users!=users)return fail(error,"Intervention Subscriber original capture changed native character definitions/users");
    error.clear();return true;
}
bool zone(const action_session::Context& context,const action_session::Binding& binding,uintptr_t object,uint64_t uid,const std::string& key,const External& external,npc_probe::character_actor_records::Result& out,std::string& error){
    out={};if(!external)return fail(error,"Intervention Zone capture requires complete canonical external owners");
    character_definition_identity::Census definitions;character_membership_reader::Users users;if(!npc_probe::character_actor_records::read_current(context,binding,definitions,users,error))return false;
    World world;if(!native_open_runtime::read_current_world(world,error))return false;Memory memory;uintptr_t shard=0,region=0;
    if(!zone_pin(memory,world,object,uid,key,shard,region,error)||!code(memory,uintptr_t(GetModuleHandleW(nullptr)),error,false))return false;
    const auto image=uintptr_t(GetModuleHandleW(nullptr));struct Body{uintptr_t rva;size_t size;const char* sha;};constexpr Body bodies[]{
        {0x10fd270,96,"16726f7062a59510b9f4182cc39d407e182028275f7c4dad599bdaec8ac61403"},
        {0x10fd2d0,464,"01ac6918fc4d79f49c08143258c586977719108259dff56b4e76654e884e442c"}};
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);if(!memory.read(image+body.rva,bytes.data(),bytes.size(),error))return false;if(mt2mp::sha256(bytes)!=body.sha)return fail(error,"Intervention original Zone serializer body differs RVA="+std::to_string(body.rva));}
    const c::CaptureApi api{reinterpret_cast<c::Construct>(image+0x9667c0),reinterpret_cast<c::Construct>(image+0x9676a0),reinterpret_cast<c::Construct>(image+0x89e580),reinterpret_cast<c::Construct>(image+0x89e5f0),reinterpret_cast<c::PrepareSave>(image+0x10fd270),reinterpret_cast<c::SaveRecord>(image+0x10fd2d0)};
    ZoneBorrow value{memory,external,object,out};{live_host_state::apply_timing::PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);
        if(!c::capture_original(api,reinterpret_cast<const void*>(object),&inspect_zone,&value,out.cleanup,error))return false;}
    uintptr_t final_shard=0,final_region=0;if(!zone_pin(memory,world,object,uid,key,final_shard,final_region,error)||final_shard!=shard||final_region!=region)return fail(error,error.empty()?"Intervention original Zone capture changed its native scene":error);
    error.clear();return true;
}
}
