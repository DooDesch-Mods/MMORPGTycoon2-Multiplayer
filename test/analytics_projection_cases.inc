namespace analytics_projection_fixture {
namespace values=mt2mp::game::analytics_state;
namespace a=mt2mp::action_session;
void need(bool okay,const std::string& cause){::need(okay,cause.c_str());}
constexpr uintptr_t image=0x100000,state_owner=0x200000,analytics_owner=0x210000,stats_owner=0x220000;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;std::map<uintptr_t,size_t> allocations;uintptr_t next=0x300000;
    uintptr_t allocate(size_t size){const auto address=next;next+=size+0x100;allocations.emplace(address,size);for(size_t i=0;i<size;++i)bytes[address+i]=0;return address;}
    void release(uintptr_t address){const auto found=allocations.find(address);if(found==allocations.end())throw std::runtime_error("fixture native duplicate/foreign resource release");for(size_t i=0;i<found->second;++i)bytes.erase(address+i);allocations.erase(found);}
    bool copy(uintptr_t address,void* out,size_t count,std::string& e){auto* target=static_cast<unsigned char*>(out);for(size_t i=0;i<count;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){e="fixture exact native byte missing";return false;}target[i]=found->second;}return true;}
    void store(uintptr_t address,const void* input,size_t count){const auto* data=static_cast<const unsigned char*>(input);for(size_t i=0;i<count;++i)bytes[address+i]=data[i];}
    template<class T>void put(uintptr_t address,const T& value){store(address,&value,sizeof value);}
    template<class T>T get(uintptr_t address){T value{};std::string e;if(!copy(address,&value,sizeof value,e))throw std::runtime_error(e);return value;}
} memory;
size_t calls=0,writes=0,fail_call=0,fail_write=0;bool world_valid=true,drift=false;
std::vector<uintptr_t> write_addresses;
namespace live {
using Pin=mt2mp::game::live_host_state::Pin;
struct ManagementReceipt {bool attempted=false,confirmed=false,unknown=false;uint32_t attempted_stores=0,stores=0;std::string cause;};
struct NativePort {NativePort(const a::Context&,const a::Binding&,void*){}bool current_owners(const Pin&,std::string& e){if(!world_valid){e="fixture changed native World";return false;}return true;}};
struct Fence {bool read_current_fence(const Pin&,std::string& e){if(!world_valid){e="fixture changed native fence";return false;}return true;}};
Fence& economic_fence(){static Fence fence;return fence;}
namespace apply_timing {enum class PageBoundary{OriginalCall};struct PageReadScope{explicit PageReadScope(PageBoundary){}};}
}
namespace native {struct WorldStatus{int saves_in_progress=0,edit_operations=0;};bool read_world_status(WorldStatus&,std::string&){return true;}}
struct Runtime{uintptr_t image=analytics_projection_fixture::image;Memory& memory=analytics_projection_fixture::memory;};
nlohmann::json last;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(Runtime& r,uintptr_t object,size_t offset,T& out,std::string& e){return r.memory.copy(object+offset,&out,sizeof out,e);}
#include "analytics_call_types.inc"
bool sample(Runtime& r,const a::Context& c,const a::Binding& b,values::Owners& owners,values::State& out,live::Pin& pin,std::string& e){
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    auto copy=[&](uintptr_t object,void* value,size_t bytes,std::string& cause){return r.memory.copy(object,value,bytes,cause);};
    if(!values::read(read,copy,image,state_owner,owners,out,e))return false;pin={};pin.context=c;pin.binding=b;pin.image=image;pin.world=world_valid?88:89;pin.thread=17;return true;
}
bool code(Runtime&,std::string&){return true;}
uintptr_t reason(const values::Reason& row){const auto object=memory.allocate(0x38);memory.put(object,image+0x1711d40);memory.put(object+0x18,row.type);memory.put(object+0x20,image+0x1746160);
    const auto count=int32_t(row.positions.size());const auto data=count?memory.allocate(size_t(count)*12):uintptr_t{0};if(count)memory.store(data,row.positions.data(),size_t(count)*12);memory.put(object+0x28,data);memory.put(object+0x30,std::array<int32_t,2>{count,count});return object;}
void fill_day(uintptr_t object,const values::Day& day){memory.put(object,image+0x172e460);memory.put(object+0x18,day.day);memory.put(object+0x20,image+0x16ec160);
    std::vector<uintptr_t> rows;for(const auto& row:day.reasons)rows.push_back(reason(row));const auto data=rows.empty()?uintptr_t{0}:memory.allocate(rows.size()*8);if(data)memory.store(data,rows.data(),rows.size()*8);memory.put(object+0x28,data);memory.put(object+0x30,std::array<int32_t,2>{int32_t(rows.size()),int32_t(rows.size())});}
values::Day read_day(uintptr_t object){values::Day day;day.day=memory.get<int32_t>(object+0x18);const auto data=memory.get<uintptr_t>(object+0x28);const auto count=memory.get<int32_t>(object+0x30);
    for(int32_t i=0;i<count;++i){const auto item=memory.get<uintptr_t>(data+size_t(i)*8);values::Reason row;row.type=memory.get<int32_t>(item+0x18);const auto positions=memory.get<uintptr_t>(item+0x28);const auto amount=memory.get<int32_t>(item+0x30);row.positions.resize(size_t(amount));std::string e;if(amount&&!memory.copy(positions,row.positions.data(),size_t(amount)*12,e))throw std::runtime_error(e);day.reasons.push_back(std::move(row));}return day;}
void clear_day(uintptr_t object,bool allocation){const auto data=memory.get<uintptr_t>(object+0x28);const auto count=memory.get<int32_t>(object+0x30);
    for(int32_t i=0;i<count;++i){const auto item=memory.get<uintptr_t>(data+size_t(i)*8);const auto positions=memory.get<uintptr_t>(item+0x28);if(positions)memory.release(positions);memory.release(item);}if(data)memory.release(data);if(allocation)memory.release(object);}
uint32_t invoke(Call& call){++calls;if(calls==fail_call)return 55;
    switch(call.op){
    case Op::Day:call.result=memory.allocate(0x50);fill_day(call.result,{});break;
    case Op::Reason:{call.result=memory.allocate(0x38);memory.put(call.result,image+0x1711d40);memory.put(call.result+0x20,image+0x1746160);const auto data=memory.allocate(48);memory.put(call.result+0x28,data);memory.put(call.result+0x30,std::array<int32_t,2>{0,4});break;}
    case Op::Allocate:call.result=memory.allocate(call.bytes);break;
    case Op::Free:memory.release(call.object);break;
    case Op::DayDestroy:clear_day(call.object,true);break;
    case Op::Assign:{const auto value=read_day(call.source);clear_day(call.object,false);fill_day(call.object,value);break;}
    }
    if(drift&&calls==1)memory.put(analytics_owner+0x50,int32_t{999});return 0;
}
uint32_t write(uintptr_t address,const void* expected,const void* desired,size_t count){++writes;write_addresses.push_back(address);if(writes==fail_write)return 66;
    std::vector<unsigned char> actual(count);std::string e;if(expected&&(!memory.copy(address,actual.data(),count,e)||std::memcmp(actual.data(),expected,count)))return 77;memory.store(address,desired,count);return 0;}
#include "analytics_adopt_actual.inc"
values::State baseline(){values::State value;value.counters={1,2,3,100,4,5,6};value.today.day=12;value.today.reasons={{2,{{std::bit_cast<uint32_t>(1.f),0,std::bit_cast<uint32_t>(3.f)}}}};for(int32_t day=2;day<12;++day)value.history.push_back({day,{{day%10,{{0,0,std::bit_cast<uint32_t>(float(day))}}}}});return value;}
void reset(const values::State& value){memory=Memory{};calls=writes=fail_call=fail_write=0;world_valid=true;drift=false;write_addresses.clear();last=nullptr;
    memory.put(image+0x13df280,analytics_owner);memory.put(state_owner+0x208,analytics_owner);memory.put(analytics_owner,image+0x16dd3b0);memory.put(analytics_owner+0x38,stats_owner);memory.put(analytics_owner+0x40,value.counters);memory.put(analytics_owner+0x5c,std::array<uint8_t,2>{1,0});memory.put(stats_owner,image+0x17386b0);
    fill_day(stats_owner+0x38,value.today);memory.put(stats_owner+0x88,image+0x16ee200);std::vector<uintptr_t> history;for(const auto& day:value.history){auto object=memory.allocate(0x50);fill_day(object,day);history.push_back(object);}auto data=history.empty()?uintptr_t{0}:memory.allocate(history.size()*8);if(data)memory.store(data,history.data(),history.size()*8);memory.put(stats_owner+0x90,data);memory.put(stats_owner+0x98,std::array<int32_t,2>{int32_t(history.size()),int32_t(history.size())});}
void run(){const auto base=baseline();auto target=base;target.counters={7,8,9,1000,10,11,12};target.history.erase(target.history.begin(),target.history.begin()+2);target.history.push_back(base.today);target.history.push_back({13,{{4,{{0,0,0},{0,0,0}}}}});target.today={14,{{7,{{0,0,0},{0,0,0},{0,0,0}}}}};
    Runtime runtime;a::Context context;context.role=a::Role::Client;a::Binding binding;std::string e;live::ManagementReceipt receipt;
    reset(base);need(adopt(runtime,context,binding,target,receipt,e)&&last.at("confirmed")==true,"actual full Statistics native projection: "+e);const auto successful_calls=calls;const auto stores=write_addresses;
    values::Owners owners;values::State actual;live::Pin pin;need(sample(runtime,context,binding,owners,actual,pin,e)&&actual==target,"complete native Statistics readback differs");need(memory.get<std::array<uint8_t,2>>(analytics_owner+0x5c)==std::array<uint8_t,2>{1,0},"local Analytics views changed");
    reset(base);receipt={};need(adopt(runtime,context,binding,base,receipt,e)&&!receipt.attempted&&calls==0&&writes==0,"unchanged Statistics performed native effects");
    auto counters=base;++counters.counters[4];reset(base);receipt={};need(adopt(runtime,context,binding,counters,receipt,e)&&calls==0&&writes==1,"counter-only Statistics rebuilt native objects");
    for(size_t failure=1;failure<=successful_calls;++failure){reset(base);fail_call=failure;receipt={};e.clear();need(!adopt(runtime,context,binding,target,receipt,e)&&receipt.attempted&&last.at("unknown")==true&&!e.empty()&&calls==failure,"actual native Statistics call failure continued or lost cause");}
    std::set<size_t> failures{1,stores.size()};for(size_t i=0;i<stores.size();++i)if(stores[i]==stats_owner+0x90||stores[i]==stats_owner+0x98)failures.insert(i+1);
    for(auto failure:failures){reset(base);fail_write=failure;receipt={};e.clear();need(!adopt(runtime,context,binding,target,receipt,e)&&receipt.attempted&&last.at("unknown")==true&&writes==failure&&!e.empty(),"actual partial Statistics ownership write continued");}
    reset(base);drift=true;receipt={};need(!adopt(runtime,context,binding,target,receipt,e)&&last.at("unknown")==true&&e.find("prior changed")!=std::string::npos,"detached factory callback drift accepted");
    reset(base);world_valid=false;receipt={};need(!adopt(runtime,context,binding,target,receipt,e)&&!receipt.attempted&&calls==0,"foreign current World mutated");
    reset(base);auto wire=values::encode(base);values::State decoded;need(values::decode(wire,decoded,e)&&decoded==base,"Statistics full typed codec roundtrip");wire["counters"].push_back(1);need(!values::decode(wire,decoded,e),"incomplete counter block accepted");
    wire=values::encode(base);wire["today"]["reasons"][0]["positions"][0][0]=uint32_t{0x7f800000};need(!values::decode(wire,decoded,e),"nonfinite native position accepted");
    reset(base);memory.put(state_owner+0x208,uintptr_t{1});need(!sample(runtime,context,binding,owners,actual,pin,e),"foreign State Analytics owner accepted");
}
}
