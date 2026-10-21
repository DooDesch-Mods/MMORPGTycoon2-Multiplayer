namespace demand_projection_fixture {
namespace values=mt2mp::game::subscriber_demand;
namespace a=mt2mp::action_session;
void need(bool okay,const std::string& cause){::need(okay,cause.c_str());}
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> blocks;
    uintptr_t allocate(size_t count){std::vector<unsigned char> value(count);const auto address=uintptr_t(value.data());blocks.emplace(address,std::move(value));return address;}
    bool copy(uintptr_t address,void* out,size_t count,std::string& e){for(auto& [base,bytes]:blocks)if(address>=base&&address-base<=bytes.size()&&count<=bytes.size()-(address-base)){std::memcpy(out,bytes.data()+(address-base),count);return true;}e="fixture native extent unavailable";return false;}
    template<class T>void put(uintptr_t address,const T& value){for(auto& [base,bytes]:blocks)if(address>=base&&address-base<=bytes.size()&&sizeof value<=bytes.size()-(address-base)){std::memcpy(bytes.data()+(address-base),&value,sizeof value);return;}throw std::runtime_error("fixture write outside current allocation");}
} memory;
uintptr_t image=0,manager=0,data=0;std::vector<uintptr_t> subscribers;std::map<uintptr_t,std::string> keys;
size_t samples=0,fail_sample=0,drift_sample=0;bool world=true;
namespace character_calendar {
using State=mt2mp::game::character_calendar::State;
State current;
bool capture(const a::Context&,const a::Binding&,State& out,std::string&){out=current;return true;}
}
namespace live {
using Pin=mt2mp::game::live_host_state::Pin;
struct ManagementReceipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
struct NativePort {NativePort(const a::Context&,const a::Binding&,void*){}bool current_owners(const Pin&,std::string& e){if(!world){e="fixture World invalidated";return false;}return true;}};
struct Fence {bool read_current_fence(const Pin&,std::string& e){if(!world){e="fixture fence changed";return false;}return true;}};
Fence& economic_fence(){static Fence value;return value;}
namespace apply_timing {enum class PageBoundary{OriginalCall};struct PageReadScope{explicit PageReadScope(PageBoundary){}};}
}
struct Runtime{};
nlohmann::json last;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool sample(Runtime&,const a::Context& c,const a::Binding& b,values::Owners& owners,values::State& out,std::map<uintptr_t,std::string>& current_keys,live::Pin& pin,std::string& e){
    ++samples;if(samples==fail_sample)return fail(e,"fixture fresh capture failure");
    if(samples==drift_sample)memory.put(subscribers.back()+0x20,int32_t{888});
    auto read=[](uintptr_t object,size_t offset,auto& value,std::string& cause){return memory.copy(object+offset,&value,sizeof value,cause);};
    if(!values::read(read,image,keys,owners,out,e))return false;
    pin={};pin.context=c;pin.binding=b;pin.image=image;pin.world=world?7:8;pin.selected=0;current_keys=keys;return true;
}
#include "subscriber_demand_apply_actual.inc"
values::State baseline(){return {{3,std::nullopt},{7,std::nullopt},{12,std::nullopt}};}
character_calendar::State calendar(){
    character_calendar::State out;mt2mp::game::character_calendar::Definition row;row.kind=row.id=0;
    row.statistics=mt2mp::game::character_calendar::Statistics{};row.demands[0]=mt2mp::game::character_calendar::Demand{"Buff",{3}};row.demands[1]=mt2mp::game::character_calendar::Demand{"Nerf",{12}};
    out.definitions.push_back(row);out.demand_order={"character:0:0/demand/buff","character:0:0/demand/nerf"};return out;
}
void reset(){memory={};subscribers.clear();samples=fail_sample=drift_sample=0;world=true;last=nullptr;
    image=memory.allocate(0x13df698);manager=memory.allocate(0x50);data=memory.allocate(24);
    memory.put(image+0x13df690,manager);memory.put(manager,image+0x1725f40);memory.put(manager+0x40,data);memory.put(manager+0x48,std::array<int32_t,2>{3,3});
    const auto base=baseline();for(size_t i=0;i<base.size();++i){auto object=memory.allocate(0xc8);subscribers.push_back(object);memory.put(data+i*8,object);memory.put(object,image+0x16e75f0);memory.put(object+0x20,base[i].id);memory.put(object+0xc0,uintptr_t{0});}
    keys={{0x30000,"character:0:0/demand/buff"},{0x40000,"character:0:0/demand/nerf"}};character_calendar::current=calendar();
}
void run(){Runtime runtime;a::Context c;c.role=a::Role::Client;a::Binding b;std::string e;live::ManagementReceipt receipt;
    auto target=baseline();target[0].key="character:0:0/demand/buff";target[2].key="character:0:0/demand/nerf";
    reset();need(apply(runtime,c,b,target,character_calendar::current,receipt,e)&&receipt.stores==2&&samples==5&&last.at("confirmed")==true,"actual reciprocal Subscriber Demand pointer adoption: "+e);
    values::Owners owners;values::State actual;std::map<uintptr_t,std::string> actual_keys;live::Pin pin;
    need(sample(runtime,c,b,owners,actual,actual_keys,pin,e)&&actual==target,"complete Demand pointer readback");
    receipt={};samples=0;need(apply(runtime,c,b,target,character_calendar::current,receipt,e)&&!receipt.attempted&&samples==1,"unchanged Demand references repeated native effects");
    reset();receipt={};fail_sample=3;need(!apply(runtime,c,b,target,character_calendar::current,receipt,e)&&receipt.attempted&&receipt.stores==1&&samples==3&&last.at("unknown")==true,"partial Demand store failure lost Unknown or continued");
    reset();receipt={};drift_sample=2;need(!apply(runtime,c,b,target,character_calendar::current,receipt,e)&&!receipt.attempted&&samples==2,"fresh complete Subscriber ID drift accepted before CAS");
    reset();receipt={};world=false;need(!apply(runtime,c,b,target,character_calendar::current,receipt,e)&&!receipt.attempted,"changed native World mutated Demand pointers");
    reset();receipt={};auto foreign=target;foreign[0].key="character:0:0/demand/nerf";need(!apply(runtime,c,b,foreign,character_calendar::current,receipt,e)&&!receipt.attempted,"nonreciprocal target accepted");
    auto wire=values::encode(target);values::State decoded;need(values::decode(wire,decoded,e)&&decoded==target,"Demand full canonical codec roundtrip");wire[0][1]="character:0:00/demand/buff";need(!values::decode(wire,decoded,e),"noncanonical Demand key accepted");
    reset();memory.put(subscribers[0]+0xc0,uintptr_t{0x99999});need(!sample(runtime,c,b,owners,actual,actual_keys,pin,e),"foreign Demand native pointer accepted");
    reset();memory.put(subscribers[1]+0x20,int32_t{3});need(!sample(runtime,c,b,owners,actual,actual_keys,pin,e),"duplicate native Subscriber ID accepted");
    reset();memory.put(manager+0x4c,int32_t{2});need(!sample(runtime,c,b,owners,actual,actual_keys,pin,e),"incomplete manager extent accepted");
    bool stored=false;need(compare_pointer(1,0,1,stored)!=0&&!stored,"invalid Demand pointer store lost SEH");
}
}
