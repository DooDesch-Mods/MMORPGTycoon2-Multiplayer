#include "character_calendar_native.h"
#include "character_calendar_apply.h"
#include "character_calendar_codes.h"
#include "character_actor_records.h"
#include "npc_census.h"
#include "world_zone_runtime.h"
#include "subscriber_intervention.h"
#include "live_host_state/native_port.h"
#include "live_host_state/apply_timing.h"
#include "live_host_state/memory_query_timing.h"
#include "session_provider.h"
#include "native_open_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/hash.h"
#include <Windows.h>
#include <intrin.h>
#include <algorithm>
#include <cstring>

namespace mt2mp::game::character_calendar {
namespace {
namespace a=action_session;namespace live=live_host_state;namespace definitions=character_definition_identity;
struct DemandOwner {uintptr_t object=0,parent=0;size_t definition=0,index=0;std::string key;};
struct Bound {
    live::Pin pin;definitions::Census definitions;character_membership_reader::Users users;
    subscriber_intervention::Owners subscribers;std::vector<world_zone_runtime::Current> zones;
    uintptr_t demand_manager=0,demand_data=0;int32_t demand_count=0,demand_capacity=0;
    std::vector<uintptr_t> demand_members;std::map<uintptr_t,DemandOwner> demands;
};
bool same_bound(const Bound& before,const Bound& after,std::string& error){
    if(before.pin!=after.pin||before.definitions.manager!=after.definitions.manager||before.definitions.state!=after.definitions.state||
       before.demand_manager!=after.demand_manager||before.demand_members!=after.demand_members||before.subscribers!=after.subscribers||
       before.definitions.definitions.size()!=after.definitions.definitions.size()||before.zones.size()!=after.zones.size())
        return fail(error,"Character calendar current complete physical owners changed");
    for(size_t i=0;i<before.definitions.arrays.size();++i){const auto& a=before.definitions.arrays[i];const auto& b=after.definitions.arrays[i];
        if(a.object!=b.object||a.data!=b.data||a.count!=b.count||a.capacity!=b.capacity||a.members!=b.members)
            return fail(error,"Character calendar current global definition storage/lifetime changed");}
    for(size_t i=0;i<before.definitions.definitions.size();++i){const auto& a=before.definitions.definitions[i];const auto& b=after.definitions.definitions[i];
        if(a.object!=b.object||a.uid!=b.uid||a.kind!=b.kind||a.id!=b.id||a.deleted!=b.deleted||a.parent!=b.parent||
           a.parent_uid!=b.parent_uid||a.owning_collection!=b.owning_collection||a.variants!=b.variants||a.record!=b.record)
            return fail(error,"Character calendar current full UID/parent/record lifetime changed");}
    for(size_t i=0;i<before.zones.size();++i){const auto& a=before.zones[i];const auto& b=after.zones[i];
        if(a.key!=b.key||a.scene_key!=b.scene_key||a.object!=b.object||a.root!=b.root||a.region!=b.region||a.manager!=b.manager||a.uid!=b.uid||a.type!=b.type)
            return fail(error,"Zone calendar current canonical/local receiver lifetime changed");}
    return true;
}
struct Memory {
    placement_ui_probe::ProcessMemory process;
    template<class T>bool at(uintptr_t base,size_t offset,T& value,std::string& error){
        if(!base||base>UINTPTR_MAX-offset||base+offset>UINTPTR_MAX-(sizeof value-1))return fail(error,"Character calendar checked native address overflows");
        return process.copy(base+offset,&value,sizeof value,error);
    }
    bool ap(uintptr_t object,uintptr_t image,uintptr_t expected,std::string& error){uintptr_t actual=0;
        if(!at(object,0,actual,error))return false;return actual==image+expected||fail(error,"Character calendar concrete native class differs");}
    bool text(uintptr_t object,size_t offset,std::string& value,std::string& error){
        uintptr_t data=0;uint64_t length=0;if(!at(object,offset,data,error)||!at(object,offset+8,length,error))return false;
        if(!data||length>65536||data>UINTPTR_MAX-length)return fail(error,"Character calendar native string extent differs");
        std::string next(size_t(length),'\0');if(length&&!process.copy(data,next.data(),size_t(length),error))return false;
        if(next.find('\0')!=std::string::npos||!npc_census::utf8(next))return fail(error,"Character calendar native string contains NUL/invalid UTF-8");value=std::move(next);return true;
    }
};
bool code(Memory& memory,uintptr_t image,std::string& error){
    for(const auto& pin:codes::values){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.process.copy(image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(error,"Character calendar original body differs RVA="+std::to_string(pin.rva));}
    return true;
}
bool bind(Memory& memory,const a::Context& context,const a::Binding& binding,Bound& out,std::string& error){
    out={};uintptr_t image=0;uint32_t thread=0;std::string sha;a::Context actual;
    if(!native::read_resolver_receipt(image,thread,sha,error)||!a::selected_session().current(actual,error))return false;
    if(actual!=context||thread!=GetCurrentThreadId()||image!=uintptr_t(GetModuleHandleW(nullptr))||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
        return fail(error,"Character calendar actual Session/Mainthread/EXE differs");
    live::NativePort port(context,binding,nullptr);
    if(!port.read(out.pin,error)||!npc_probe::character_actor_records::read_current(context,binding,out.definitions,out.users,error)||!code(memory,image,error))return false;
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return memory.at(base,offset,value,cause);};
    auto copy=[&](uintptr_t address,void* target,size_t bytes,std::string& cause){return memory.process.copy(address,target,bytes,cause);};
    subscriber_intervention::State ignored;
    if(!subscriber_intervention::read_all(read,copy,image,ignored,out.subscribers,error)||
       !memory.at(image,0x13df470,out.demand_manager,error)||!memory.ap(out.demand_manager,image,0x1705e00,error)||
       !memory.at(out.demand_manager,0x10,out.demand_data,error)||!memory.at(out.demand_manager,0x18,out.demand_count,error)||
       !memory.at(out.demand_manager,0x1c,out.demand_capacity,error))return false;
    if(out.demand_count<0||out.demand_count>200000||out.demand_capacity<out.demand_count||
       (out.demand_count&&(!out.demand_data||out.demand_data>UINTPTR_MAX-size_t(out.demand_count)*8)))return fail(error,"Character DemandManager complete member extent differs");
    std::map<uintptr_t,DemandOwner> expected;
    for(size_t i=0;i<out.definitions.definitions.size();++i){const auto& row=out.definitions.definitions[i];
        for(size_t index=0;index<2;++index){const auto offset=index?size_t(0x398):size_t(0x310);
            if(row.object>UINTPTR_MAX-offset)return fail(error,"Character embedded Demand owner extent overflows");
            DemandOwner owner{row.object+offset,row.object,i,index,definition_key(row.kind,row.id)+(index?"/demand/nerf":"/demand/buff")};
            if(!expected.emplace(owner.object,std::move(owner)).second)return fail(error,"Character embedded Demand receiver aliases another original definition");}}
    for(int32_t i=0;i<out.demand_count;++i){uintptr_t object=0,parent=0;if(!memory.at(out.demand_data,size_t(i)*8,object,error))return false;
        const auto found=expected.find(object);if(found==expected.end())return fail(error,"Character DemandManager contains an unbound original owner");
        const auto& owner=found->second;
        if(!memory.ap(object,image,owner.index?0x173c4d0:0x173c3f0,error)||!memory.ap(object+0x38,image,0x1717130,error)||
           !memory.ap(object+0x50,image,0x17171e8,error)||!memory.at(object,0x80,parent,error))return false;
        if(parent!=owner.parent||!out.demands.emplace(object,owner).second)return fail(error,"Character registered Demand parent/order is foreign or repeated");
        out.demand_members.push_back(object);
    }
    return port.current_owners(out.pin,error);
}
bool integers(Memory& memory,uintptr_t image,uintptr_t object,std::vector<int32_t>& out,int32_t maximum,std::string& error){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!memory.ap(object,image,0x1749370,error)||!memory.at(object,8,data,error)||!memory.at(object,0x10,count,error)||!memory.at(object,0x14,capacity,error))return false;
    if(count<0||count>maximum||capacity<count||(count&&(!data||data>UINTPTR_MAX-size_t(count)*4)))return fail(error,"Character calendar integer-array extent differs");
    std::vector<int32_t> next(size_t(count),int32_t{});if(count&&!memory.process.copy(data,next.data(),next.size()*4,error))return false;
    uintptr_t final_data=0;int32_t final_count=0,final_capacity=0;
    if(!memory.at(object,8,final_data,error)||!memory.at(object,0x10,final_count,error)||!memory.at(object,0x14,final_capacity,error))return false;
    if(final_data!=data||final_count!=count||final_capacity!=capacity)return fail(error,"Character calendar integer array changed during read");out=std::move(next);return true;
}
bool rating(Memory& memory,uintptr_t image,uintptr_t object,Rating& out,std::string& error){
    uint8_t present=2;if(!memory.ap(object,image,0x1787870,error)||!memory.at(object,0x18,out.bits,error)||!memory.at(object,0x24,present,error))return false;
    if(present>1)return fail(error,"Character/Zone native rating flag is not Boolean");out.set=present!=0;return valid(out,error);
}
bool once(Memory& memory,const a::Context& context,const a::Binding& binding,Bound& local,State& out,std::string& error,bool with_zones){
    live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());
    out={};if(!bind(memory,context,binding,local,error))return false;const auto image=local.pin.image;
    std::map<uintptr_t,int32_t> subscribers;
    for(const auto& [object,row]:local.subscribers.rows)subscribers.emplace(object,row.id);
    for(size_t i=0;i<local.definitions.definitions.size();++i){const auto& native=local.definitions.definitions[i];Definition row;
        row.kind=native.kind;row.id=native.id;row.deleted=native.deleted;
        if(!row.deleted){Statistics stats;uint8_t tier_valid=2;
            if(!memory.at(native.object,0x2d0,stats.tiers,error)||!memory.at(native.object,0x2d8,tier_valid,error)||!memory.at(native.object,0x2dc,stats.last_edit_days,error)||
               !rating(memory,image,native.object+0x2e8,stats.rating,error)||!memory.at(native.object,0x4b0,stats.optimal_dps,error))return false;
            if(tier_valid>1)return fail(error,"Character original tier-valid flag is not Boolean");stats.tier_valid=tier_valid!=0;
            for(size_t index=0;index<2;++index){const auto object=native.object+(index?0x458:0x420);auto& history=stats.durations[index];
                if(!memory.ap(object,image,0x17138d0,error)||!memory.at(object,0x30,history.limit,error)||history.limit<0||history.limit>1000000||
                   !integers(memory,image,object+0x18,history.values,history.limit,error))return fail(error,error.empty()?"Character IntegerHistory original policy differs":error);}
            row.statistics=std::move(stats);
        }
        for(size_t index=0;index<2;++index){const auto object=native.object+(index?0x398:0x310);
            if(!local.demands.contains(object))continue;Demand demand;uintptr_t data=0;int32_t count=0,capacity=0;
            if(!memory.text(object,0x18,demand.name,error)||!memory.at(object,0x58,data,error)||!memory.at(object,0x60,count,error)||!memory.at(object,0x64,capacity,error))return false;
            if(count<0||count>1000000||capacity<count||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))return fail(error,"Character Demand member storage differs");
            for(int32_t member=0;member<count;++member){uintptr_t subscriber=0;if(!memory.at(data,size_t(member)*8,subscriber,error))return false;
                const auto found=subscribers.find(subscriber);if(found==subscribers.end())return fail(error,"Character Demand member lacks current saved Subscriber identity");demand.members.push_back(found->second);}
            uintptr_t current_data=0;int32_t current_count=0,current_capacity=0;
            if(!memory.at(object,0x58,current_data,error)||!memory.at(object,0x60,current_count,error)||!memory.at(object,0x64,current_capacity,error))return false;
            if(current_data!=data||current_count!=count||current_capacity!=capacity)return fail(error,"Character Demand member storage changed during read");
            row.demands[index]=std::move(demand);
        }
        out.definitions.push_back(std::move(row));
    }
    for(const auto object:local.demand_members)out.demand_order.push_back(local.demands.at(object).key);
    if(with_zones){if(!world_zone_runtime::read_current(binding,context.snapshot_uuid,local.zones,error))return false;
        std::sort(local.zones.begin(),local.zones.end(),[](const auto& a,const auto& b){return a.key<b.key;});
        for(const auto& native:local.zones){Zone row;row.key=native.key;row.scene=native.scene_key;row.type=native.type;
            uint64_t uid=0;uintptr_t region=0;if(!memory.ap(native.object,image,0x1745ce0,error)||!memory.at(native.object,0x18,uid,error)||
               !memory.at(native.object,0x178,region,error)||uid!=native.uid||region!=native.region)return fail(error,error.empty()?"Zone calendar current full UID/Region owner differs":error);
            if(!memory.at(native.object,0x564,row.constructed,error)||!memory.at(native.object,0x570,row.counts,error)||
               !integers(memory,image,native.object+0x5c8,row.kills[0],10,error)||!integers(memory,image,native.object+0x5e0,row.kills[1],10,error)||
               !rating(memory,image,native.object+0x5f8,row.rating,error))return false;
            out.zones.push_back(std::move(row));}
    }
    live::NativePort port(context,binding,nullptr);return valid(out,error)&&port.current_owners(local.pin,error);
}
uint32_t compare(uintptr_t address,unsigned bytes,uint32_t old,uint32_t desired,bool& stored)noexcept{
    __try{if(bytes==1)stored=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(desired),char(old)))==uint8_t(old);
        else stored=uint32_t(InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(address),LONG(desired),LONG(old)))==old;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t reserve(uintptr_t image,uintptr_t object,int32_t count)noexcept{
    __try{reinterpret_cast<void(*)(void*,int32_t)>(image+0xe8080)(reinterpret_cast<void*>(object),count);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t demand_call(uintptr_t image,uintptr_t object,uintptr_t subscriber,bool add)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(image+(add?0x25cb30:0x25d480))(reinterpret_cast<void*>(object),reinterpret_cast<void*>(subscriber));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Port {
    Memory memory;a::Context context;a::Binding binding;Bound local;bool bound=false;
    bool sample(State& out,std::string& error){Bound fresh;
        if(!once(memory,context,binding,fresh,out,error,true))return false;
        if(bound&&!same_bound(local,fresh,error))return false;
        local=std::move(fresh);bound=true;return true;
    }
    bool preflight(const State& target,std::string& error){
        if(context.role!=a::Role::Client||local.pin.selected!=0)return fail(error,"Character calendar projection requires joined Client original Clock0");
        std::set<int32_t> ids;for(const auto& [object,row]:local.subscribers.rows){(void)object;ids.insert(row.id);}
        for(const auto& row:target.definitions)for(const auto& demand:row.demands)if(demand)for(auto id:demand->members)
            if(!ids.contains(id))return fail(error,"Character Demand target references a Subscriber not yet admitted by original lifecycle");
        return true;
    }
    bool guard(std::string& error){
        live::NativePort port(context,binding,nullptr);if(!port.current_owners(local.pin,error))return false;
        auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return memory.at(base,offset,value,cause);};
        for(const auto& array:local.definitions.arrays)if(!definitions::unchanged(read,array,error))return false;
        for(const auto& row:local.definitions.definitions){uint64_t uid=0;uint8_t deleted=2;
            if(!memory.at(row.object,0x18,uid,error)||!memory.at(row.object,0x490,deleted,error))return false;
            if(uid!=row.uid||deleted!=uint8_t(row.deleted))return fail(error,"Character calendar current original definition full UID/tombstone changed");}
        uintptr_t manager=0,data=0;int32_t count=0;
        if(!memory.at(local.pin.image,0x13df470,manager,error)||!memory.at(manager,0x10,data,error)||!memory.at(manager,0x18,count,error))return false;
        if(manager!=local.demand_manager||count!=local.demand_count)return fail(error,"Character current DemandManager lifetime/count changed");
        for(size_t i=0;i<local.demand_members.size();++i){uintptr_t object=0;if(!memory.at(data,i*8,object,error))return false;
            if(object!=local.demand_members[i])return fail(error,"Character current DemandManager member order changed");}
        return true;
    }
    bool store(uintptr_t address,unsigned bytes,uint32_t before,uint32_t after,Receipt& receipt,std::string& error){
        if(before==after)return true;if(!guard(error))return false;receipt.attempted=true;++receipt.attempted_stores;bool stored=false;
        const auto exception=compare(address,bytes,before,after,stored);
        if(exception||!stored)return fail(error,"Character calendar fixed scalar CAS failed SEH="+std::to_string(exception)+" stored="+std::to_string(stored));++receipt.stores;return true;
    }
    bool history(uintptr_t object,const std::vector<int32_t>& before,const std::vector<int32_t>& target,Receipt& receipt,std::string& error){
        if(before==target)return true;std::vector<int32_t> actual;if(!guard(error)||!integers(memory,local.pin.image,object,actual,1000000,error))return false;
        if(actual!=before)return fail(error,"Character calendar integer history changed before original Reserve");
        int32_t capacity=0;if(!memory.at(object,0x14,capacity,error))return false;
        if(size_t(capacity)<target.size()){receipt.attempted=true;++receipt.attempted_stores;const auto exception=reserve(local.pin.image,object,int32_t(target.size()));
            if(exception)return fail(error,"Character calendar original Reserve exception="+std::to_string(exception)+"; no replay");++receipt.stores;}
        uintptr_t data=0;int32_t count=0;
        if(!memory.at(object,8,data,error)||!memory.at(object,0x10,count,error)||!memory.at(object,0x14,capacity,error))return false;
        if(count!=int32_t(before.size())||capacity<int32_t(target.size())||(capacity&&(!data||data>UINTPTR_MAX-size_t(capacity)*4)))return fail(error,"Character calendar original Reserve readback extent differs");
        for(size_t i=0;i<target.size();++i){int32_t old=0;if(!memory.at(data,i*4,old,error))return false;
            if(i<before.size()&&old!=before[i])return fail(error,"Character calendar retained integer history prefix changed");
            if(!store(data+i*4,4,uint32_t(old),uint32_t(target[i]),receipt,error))return false;}
        return store(object+0x10,4,uint32_t(count),uint32_t(target.size()),receipt,error);
    }
    bool rating(uintptr_t object,const Rating& before,const Rating& target,Receipt& receipt,std::string& error){
        for(size_t i=0;i<3;++i)if(!store(object+0x18+i*4,4,before.bits[i],target.bits[i],receipt,error))return false;
        return store(object+0x24,1,uint32_t(before.set),uint32_t(target.set),receipt,error);
    }
    bool statistics(size_t index,const Statistics& before,const Statistics& target,Receipt& receipt,std::string& error){
        const auto object=local.definitions.definitions.at(index).object;
        for(size_t i=0;i<2;++i)if(!store(object+0x2d0+i*4,4,uint32_t(before.tiers[i]),uint32_t(target.tiers[i]),receipt,error)||
            !store(object+0x2dc+i*4,4,uint32_t(before.last_edit_days[i]),uint32_t(target.last_edit_days[i]),receipt,error)||
            !history(object+(i?0x458:0x420)+0x18,before.durations[i].values,target.durations[i].values,receipt,error))return false;
        return store(object+0x2d8,1,uint32_t(before.tier_valid),uint32_t(target.tier_valid),receipt,error)&&
            rating(object+0x2e8,before.rating,target.rating,receipt,error)&&store(object+0x4b0,4,before.optimal_dps,target.optimal_dps,receipt,error);
    }
    bool demand(size_t definition,size_t index,const Demand& before,const Demand& target,Receipt& receipt,std::string& error){
        const auto object=local.definitions.definitions.at(definition).object+(index?0x398:0x310);
        if(!local.demands.contains(object))return fail(error,"Character Demand registered original receiver disappeared");
        std::map<int32_t,uintptr_t> ids;for(const auto& [subscriber,row]:local.subscribers.rows)ids.emplace(row.id,subscriber);
        auto expected=before.members;
        const auto current=[&]{
            auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return memory.at(base,offset,value,cause);};
            auto copy=[&](uintptr_t address,void* value,size_t bytes,std::string& cause){return memory.process.copy(address,value,bytes,cause);};
            subscriber_intervention::State values;subscriber_intervention::Owners owners;
            if(!subscriber_intervention::read_all(read,copy,local.pin.image,values,owners,error))return false;
            if(owners!=local.subscribers)return fail(error,"Character Demand current Subscriber receiver/ID membership changed");
            uintptr_t data=0;int32_t count=0,capacity=0;
            if(!memory.at(object,0x58,data,error)||!memory.at(object,0x60,count,error)||!memory.at(object,0x64,capacity,error))return false;
            if(count!=int32_t(expected.size())||capacity<count||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))
                return fail(error,"Character Demand actual original member extent changed before effect");
            for(size_t i=0;i<expected.size();++i){uintptr_t actual=0;if(!memory.at(data,i*8,actual,error))return false;
                if(actual!=ids.at(expected[i]))return fail(error,"Character Demand actual original ordered member changed before effect");}
            return true;
        };
        const auto call=[&](int32_t id,bool add){const auto found=ids.find(id);
            if(found==ids.end())return fail(error,"Character Demand current Subscriber identity disappeared");
            if(!guard(error)||!current())return false;
            receipt.attempted=true;++receipt.attempted_stores;const auto exception=demand_call(local.pin.image,object,found->second,add);
            if(exception)return fail(error,"Character Demand original membership exception="+std::to_string(exception)+"; no replay");
            if(add)expected.push_back(id);else expected.erase(expected.begin());
            if(!current())return false;++receipt.stores;return true;};
        for(auto id:before.members)if(!call(id,false))return false;
        for(auto id:target.members)if(!call(id,true))return false;
        return true;
    }
    bool zone(size_t index,const Zone& before,const Zone& target,Receipt& receipt,std::string& error){
        const auto object=local.zones.at(index).object;
        for(size_t i=0;i<3;++i)if(!store(object+0x570+i*4,4,uint32_t(before.counts[i]),uint32_t(target.counts[i]),receipt,error))return false;
        return history(object+0x5c8,before.kills[0],target.kills[0],receipt,error)&&history(object+0x5e0,before.kills[1],target.kills[1],receipt,error)&&
            rating(object+0x5f8,before.rating,target.rating,receipt,error);
    }
};
}
bool capture(const a::Context& context,const a::Binding& binding,State& out,std::string& error)try{
    Memory memory;Bound first,second;State before,after;
    if(!once(memory,context,binding,first,before,error,true)||!once(memory,context,binding,second,after,error,true))return false;
    if(!same_bound(first,second,error))return false;
    if(before!=after)return fail(error,"Character/Zone calendar complete current state changed during double read");out=std::move(after);error.clear();return true;
}catch(const std::exception& exception){return fail(error,std::string("Character calendar capture: ")+exception.what());}
bool capture_demands(const a::Context& context,const a::Binding& binding,const live::Pin& expected,
                     State& out,subscriber_demand::State& demands,std::string& error)try{
    Memory memory;Bound first,second;State before,after;
    subscriber_demand::Owners first_owners,second_owners;subscriber_demand::State first_values,second_values;
    // Each pointer sample uses the complete owners admitted by its adjacent
    // calendar read. No owner set survives this synchronous pair of reads.
    const auto read_demands=[&](const Bound& local,subscriber_demand::Owners& owners,subscriber_demand::State& values){
        if(local.pin!=expected)return fail(error,"Management Character/Subscriber Demand complete Clock/Ledger origin differs");
        std::map<uintptr_t,std::string> keys;
        for(const auto& [object,row]:local.demands)keys.emplace(object,row.key);
        auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.at(object,offset,value,cause);};
        live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());
        if(!subscriber_demand::read(read,local.pin.image,keys,owners,values,error))return false;
        const auto& admitted=local.subscribers;
        if(owners.manager!=admitted.manager||owners.data!=admitted.data||owners.count!=admitted.count||
           owners.capacity!=admitted.capacity||owners.rows.size()!=admitted.rows.size())
            return fail(error,"Subscriber Demand current manager storage differs from the admitted Calendar owners");
        std::map<int32_t,uintptr_t> admitted_ids;
        for(const auto& [object,row]:admitted.rows)if(!admitted_ids.emplace(row.id,object).second)
            return fail(error,"Subscriber Demand admitted Calendar owner ID is duplicated");
        for(const auto& row:owners.rows){const auto found=admitted_ids.find(row.id);
            if(found==admitted_ids.end()||found->second!=row.object)
                return fail(error,"Subscriber Demand current allocation/ID differs from the admitted Calendar owners");}
        return true;
    };
    if(!once(memory,context,binding,first,before,error,true)||!read_demands(first,first_owners,first_values)||
       !once(memory,context,binding,second,after,error,true)||!read_demands(second,second_owners,second_values))return false;
    if(!same_bound(first,second,error))return false;
    if(before!=after)return fail(error,"Character/Zone calendar complete current state changed during double read");
    if(first_owners!=second_owners||first_values!=second_values)
        return fail(error,"Subscriber Demand complete current values/owners changed during double read");
    if(!subscriber_demand::reciprocal(second_values,after,error))return false;
    live::NativePort port(context,binding,nullptr);if(!port.current_owners(second.pin,error))return false;
    out=std::move(after);demands=std::move(second_values);error.clear();return true;
}catch(const std::exception& exception){return fail(error,std::string("Character/Subscriber Demand capture: ")+exception.what());}
bool apply(const a::Context& context,const a::Binding& binding,const State& target,Receipt& receipt,std::string& error)try{
    Port port;port.context=context;port.binding=binding;return project(port,target,receipt,error);
}catch(const std::exception& exception){receipt.unknown=receipt.attempted;receipt.cause=std::string("Character calendar projection: ")+exception.what();error=receipt.cause;return false;}
bool demand_keys(const a::Context& context,const a::Binding& binding,std::map<uintptr_t,std::string>& out,std::string& error)try{
    Memory memory;Bound local;if(!bind(memory,context,binding,local,error))return false;std::map<uintptr_t,std::string> next;
    for(const auto& [object,row]:local.demands)next.emplace(object,row.key);out=std::move(next);error.clear();return true;
}catch(const std::exception& exception){return fail(error,std::string("Character Demand canonical owner read: ")+exception.what());}
bool resolve_demand(const a::Context& context,const a::Binding& binding,const std::string& key,uintptr_t& object,std::string& error){
    object=0;std::map<uintptr_t,std::string> keys;if(!demand_keys(context,binding,keys,error))return false;
    for(const auto& [pointer,current]:keys)if(current==key){object=pointer;return true;}return fail(error,"Character Demand key has no actual current registered owner");
}
}
