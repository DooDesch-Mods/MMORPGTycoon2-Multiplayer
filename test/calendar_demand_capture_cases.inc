namespace calendar_demand_fixture {
namespace a=mt2mp::action_session;
namespace definitions=mt2mp::game::character_definition_identity;
namespace character_membership_reader=mt2mp::game::character_membership_reader;
namespace subscriber_intervention=mt2mp::game::subscriber_intervention;
namespace subscriber_demand=mt2mp::game::subscriber_demand;
namespace world_zone_runtime=mt2mp::game::world_zone_runtime;
using State=mt2mp::game::character_calendar::State;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
unsigned reads=0,binds=0,final_checks=0,reject_once=0;
bool current=true,throw_read=false;
std::function<void(unsigned)> on_once;
namespace live {
using Pin=mt2mp::game::live_host_state::Pin;
namespace apply_timing {
unsigned depth=0;
struct PageReadScope {
    explicit PageReadScope(bool){++depth;}
    ~PageReadScope(){--depth;}
    static bool reader_active(){return depth!=0;}
};
}
struct NativePort {
    NativePort(const a::Context&,const a::Binding&,void*){}
    bool current_owners(const Pin&,std::string& error){++final_checks;
        if(apply_timing::depth)return fail(error,"fixture page scope spans final World read");
        return current||fail(error,"fixture final World/SaveScope changed");}
};
}
#include "calendar_demand_owners_actual.inc"
constexpr uintptr_t image=0x10000000,manager=0x20000000,data=0x20001000;
constexpr uintptr_t first_subscriber=0x20002000,second_subscriber=0x20003000;
constexpr uintptr_t buff=0x30000310,nerf=0x30000398;
std::map<uintptr_t,std::vector<unsigned char>> fields;
template<class T>void put(uintptr_t address,const T& value){
    auto& bytes=fields[address];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);
}
struct Memory {
    template<class T>bool at(uintptr_t base,size_t offset,T& value,std::string& error){
        ++reads;if(throw_read)throw std::runtime_error("fixture original memory failure");
        if(live::apply_timing::depth!=1)return fail(error,"fixture pure pointer read lost local page scope");
        const auto found=fields.find(base+offset);
        if(found==fields.end()||found->second.size()!=sizeof value)return fail(error,"fixture native field unavailable");
        std::memcpy(&value,found->second.data(),sizeof value);return true;
    }
};
std::array<Bound,2> owner_samples;
std::array<State,2> state_samples;
bool once(Memory&,const a::Context&,const a::Binding&,Bound& local,State& out,std::string& error,bool with_zones){
    ++binds;if(on_once)on_once(binds);
    if(live::apply_timing::depth)return fail(error,"fixture page scope spans complete Calendar admission");
    if(binds==reject_once)return fail(error,"fixture original complete UID/World admission failed");
    if(!with_zones)return fail(error,"fixture complete Zone sample omitted");
    const auto index=(binds-1)%2;local=owner_samples[index];out=state_samples[index];return true;
}
#include "calendar_demand_capture_actual.inc"
live::Pin reset(){
    fields.clear();reads=binds=final_checks=reject_once=0;current=true;throw_read=false;on_once={};
    live::Pin pin;pin.image=image;pin.world=4;pin.selected=0;
    Bound owners;owners.pin=pin;owners.definitions.manager=0x30000000;owners.definitions.state=0x40000000;
    definitions::Definition definition;definition.object=0x30000000;definition.kind=definition.id=0;definition.uid=17;
    owners.definitions.definitions.push_back(definition);
    owners.demands.emplace(buff,DemandOwner{buff,definition.object,0,0,"character:0:0/demand/buff"});
    owners.demands.emplace(nerf,DemandOwner{nerf,definition.object,0,1,"character:0:0/demand/nerf"});
    owners.demand_manager=0x50000000;owners.demand_members={nerf,buff};
    owners.subscribers.manager=manager;owners.subscribers.data=data;owners.subscribers.count=owners.subscribers.capacity=2;
    subscriber_intervention::Row first;first.id=10;subscriber_intervention::Row second;second.id=20;
    owners.subscribers.rows={{first_subscriber,first},{second_subscriber,second}};
    world_zone_runtime::Current zone;zone.key="zone:actual:1";zone.scene_key="root:67";zone.uid=31;zone.object=0x60000000;
    owners.zones.push_back(zone);owner_samples={owners,owners};state_samples={::initial(),::initial()};
    put(image+0x13df690,manager);put(manager,image+0x1725f40);put(manager+0x40,data);
    put(manager+0x48,int32_t{2});put(manager+0x4c,int32_t{2});
    put(data,first_subscriber);put(data+8,second_subscriber);
    put(first_subscriber,image+0x16e75f0);put(first_subscriber+0x20,int32_t{10});put(first_subscriber+0xc0,buff);
    put(second_subscriber,image+0x16e75f0);put(second_subscriber+0x20,int32_t{20});put(second_subscriber+0xc0,nerf);
    return pin;
}
void run(){
    a::Context context;a::Binding binding;std::string error;State calendar;subscriber_demand::State demands;
    auto pin=reset();
    expect(capture_demands(context,binding,pin,calendar,demands,error)&&calendar==::initial()&&
           demands==subscriber_demand::State{{10,std::string("character:0:0/demand/buff")},{20,std::string("character:0:0/demand/nerf")}},
           "actual paired capture lost full Calendar or reciprocal Subscriber links");
    expect(binds==2&&reads==26&&final_checks==1,"paired capture did not perform exactly two fresh bindings and two actual pointer censuses");
    expect(capture_demands(context,binding,pin,calendar,demands,error)&&binds==4&&reads==52&&final_checks==2,
           "paired capture retained owners across calls");
    for(auto& state:state_samples)++state.zones[0].counts[0];
    expect(capture_demands(context,binding,pin,calendar,demands,error)&&binds==6&&calendar.zones[0].counts[0]==13,
           "paired capture did not reread state after a completed caller phase");
    pin=reset();put(data,second_subscriber);put(data+8,first_subscriber);
    for(auto& sample:owner_samples)std::swap(sample.subscribers.rows[0],sample.subscribers.rows[1]);
    expect(capture_demands(context,binding,pin,calendar,demands,error)&&demands.front().id==10,
           "paired capture confused native Subscriber order with canonical sorted IDs");
    for(unsigned fault=0;fault<19;++fault){
        pin=reset();calendar={};demands={{99,std::nullopt}};error.clear();
        if(fault==0)reject_once=1;
        if(fault==1)reject_once=2;
        if(fault==2)++owner_samples[1].pin.world;
        if(fault==3)++owner_samples[1].definitions.definitions[0].uid;
        if(fault==4)++owner_samples[1].definitions.arrays[0].data;
        if(fault==5)++owner_samples[1].zones[0].uid;
        if(fault==6)++state_samples[1].zones[0].counts[0];
        if(fault==7)owner_samples[1].demand_members.pop_back();
        if(fault==8)put(first_subscriber+0xc0,uintptr_t{0xdeadbeef});
        if(fault==9)put(first_subscriber+0x20,int32_t{20});
        if(fault==10)put(first_subscriber,image);
        if(fault==11)put(manager+0x4c,int32_t{1});
        if(fault==12)++owner_samples[0].subscribers.data;
        if(fault==13)++owner_samples[0].subscribers.rows[0].first;
        if(fault==14)current=false;
        if(fault==15)throw_read=true;
        if(fault==16)on_once=[](unsigned count){if(count==2)put(first_subscriber+0xc0,uintptr_t{0});};
        if(fault==17){state_samples[0].definitions[0].demands[0]->members.clear();state_samples[1]=state_samples[0];}
        if(fault==18)++pin.world;
        expect(!capture_demands(context,binding,pin,calendar,demands,error)&&!error.empty()&&calendar.definitions.empty()&&
               demands==subscriber_demand::State{{99,std::nullopt}},
               "paired capture published after owner/UID/World/state/reference/extent/exception failure");
        if(fault<2)expect(error=="fixture original complete UID/World admission failed","paired capture lost original admission failure");
        if(fault==14)expect(error=="fixture final World/SaveScope changed","paired capture lost final owner failure");
        if(fault==15)expect(error=="Character/Subscriber Demand capture: fixture original memory failure","paired capture lost original exception");
    }
    pin=reset();owner_samples[0].subscribers.rows.clear();owner_samples[1].subscribers.rows.clear();
    owner_samples[0].subscribers.count=owner_samples[1].subscribers.count=0;
    put(manager+0x48,int32_t{0});
    for(auto& state:state_samples)for(auto& demand:state.definitions[0].demands)demand->members.clear();
    expect(capture_demands(context,binding,pin,calendar,demands,error)&&demands.empty()&&!calendar.definitions.empty()&&!calendar.zones.empty()&&binds==2,
           "zero Subscriber case omitted live definitions/Zones or skipped fresh double admission");
}
}
