namespace thought_frame_fixture {
namespace values=mt2mp::game::toon_thought;
namespace a=mt2mp::action_session;
namespace live=analytics_projection_fixture::live;
using Memory=analytics_projection_fixture::Memory;
constexpr uintptr_t image=0x100000,toon_a=0x200000,toon_b=0x202000,building=0x204000,zone=0x206000;
Memory memory;size_t calls=0,writes=0,fail_call=0,fail_write=0;bool world_valid=true,drift=false;
std::map<int32_t,int32_t> registrations;
std::vector<values::Row> constructed;
void need(bool okay,const std::string& cause){::need(okay,cause.c_str());}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Runtime{uintptr_t image=thought_frame_fixture::image;Memory& memory=thought_frame_fixture::memory;};
template<class T>bool at(Runtime& r,uintptr_t object,size_t offset,T& value,std::string& e){return r.memory.copy(object+offset,&value,sizeof value,e);}
struct Bound{live::Pin pin;values::Bindings bindings;bool operator==(const Bound&)const=default;};
struct Snapshot{Bound bound;values::Owners owners;values::State value;};
nlohmann::json last;
values::Bindings bindings(){values::Bindings value;value.toons={{"toon:1",toon_a},{"toon:2",toon_b}};value.targets[0]={{"building",building}};value.targets[1]={{"zone",zone}};return value;}
bool sample(Runtime& r,const a::Context&,const a::Binding&,Snapshot& out,std::string& e){out={};if(!world_valid)return fail(e,"fixture fresh World changed");out.bound.bindings=bindings();out.bound.pin.image=image;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    values::Owners first;values::State before;if(!values::read(read,image,out.bound.bindings,first,before,e)||!values::read(read,image,out.bound.bindings,out.owners,out.value,e))return false;
    return (first==out.owners&&before==out.value)||fail(e,"fixture complete double read changed");}
bool code(Runtime&,std::string&){return true;}
#include "thought_frame_calls.inc"
values::Array array_at(uintptr_t owner,size_t offset){return {owner,memory.get<uintptr_t>(owner+offset),memory.get<int32_t>(owner+offset+8),memory.get<int32_t>(owner+offset+12)};}
void link(uintptr_t target,int32_t kind,uintptr_t object){const size_t base=kind?0x580:0xce0;const auto array=array_at(target,base+0x20);if(array.count>=array.capacity)throw std::runtime_error("fixture original reverse list capacity exhausted");memory.put(array.data+size_t(array.count)*8,object);memory.put(target+base+0x28,array.count+1);}
void unlink(uintptr_t target,int32_t kind,uintptr_t object){const size_t base=kind?0x580:0xce0;const auto array=array_at(target,base+0x20);int32_t slot=-1;for(int32_t i=0;i<array.count;++i)if(memory.get<uintptr_t>(array.data+size_t(i)*8)==object){if(slot>=0)throw std::runtime_error("fixture repeated reverse membership");slot=i;}
    if(slot<0)throw std::runtime_error("fixture original destructor reverse membership absent");for(int32_t i=slot+1;i<array.count;++i)memory.put(array.data+size_t(i-1)*8,memory.get<uintptr_t>(array.data+size_t(i)*8));memory.put(target+base+0x28,array.count-1);}
void destroy(uintptr_t object){constexpr std::array<size_t,3> weak{0x28,0x60,0x98};for(size_t kind=0;kind<3;++kind){const auto proxy=memory.get<uintptr_t>(object+weak[kind]+0x30);const auto target=memory.get<uintptr_t>(object+weak[kind]+0x28);if(proxy){if(kind<2&&memory.get<uintptr_t>(proxy))unlink(target,int32_t(kind),object);memory.release(proxy);}}
    if(memory.get<uint8_t>(object+0x104))--registrations[memory.get<int32_t>(object+0x20)];memory.release(object);}
uint32_t invoke(Call& call){++calls;if(calls==fail_call)return 55;
    switch(call.op){
    case Op::Allocate:case Op::AllocateArray:call.result=memory.allocate(call.bytes);break;
    case Op::FreeArray:case Op::TimeDestroy:memory.release(call.object);break;
    case Op::Time:memory.put(call.object,image+0x1744d10);memory.put(call.object+0x18,call.value->time);break;
    case Op::Thought:{memory.put(call.object,image+0x16fdef0);memory.put(call.object+0x18,call.owner);memory.put(call.object+0x20,call.value->type);memory.put(call.object+0xd0,image+0x1744d10);memory.put(call.object+0xe8,memory.get<std::array<int32_t,4>>(call.time+0x18));memory.put(call.object+0xf8,std::array<uint32_t,3>{});
        constexpr std::array<size_t,3> weak{0x28,0x60,0x98};constexpr std::array<uintptr_t,3> classes{0x17116f0,0x1711b70,0x1711ab0};for(size_t kind=0;kind<3;++kind){memory.put(call.object+weak[kind],image+classes[kind]);uintptr_t target=0,proxy=0;if(call.kind==int32_t(kind)){target=call.target;proxy=memory.allocate(8);memory.put(proxy,target);}memory.put(call.object+weak[kind]+0x28,target);memory.put(call.object+weak[kind]+0x30,proxy);}
        const uint8_t registered=call.value->type?1:0;memory.put(call.object+0x104,registered);if(registered)++registrations[call.value->type];constructed.push_back(*call.value);break;}
    case Op::Clear:{const auto array=array_at(call.object,0x8c8);for(int32_t i=0;i<array.count;++i){const auto object=memory.get<uintptr_t>(array.data+size_t(i)*8);destroy(object);memory.put(array.data+size_t(i)*8,uintptr_t{0});}memory.put(call.object+0x8d0,int32_t{0});break;}
    case Op::Notify:link(call.target,call.kind,call.object);break;
    case Op::Register:++registrations[memory.get<int32_t>(call.object+0x20)];break;
    case Op::Deregister:--registrations[memory.get<int32_t>(call.object+0x20)];break;
    case Op::ThoughtDestroy:destroy(call.object);break;
    }if(drift&&calls==1)world_valid=false;return 0;
}
uint32_t write(uintptr_t address,const void* expected,const void* target,size_t count){++writes;if(writes==fail_write)return 66;std::vector<unsigned char> prior(count);std::string e;if(expected&&(!memory.copy(address,prior.data(),count,e)||std::memcmp(prior.data(),expected,count)))return 77;memory.store(address,target,count);return 0;}
namespace analytics_native {uint32_t write(uintptr_t address,const void* expected,const void* target,size_t count){return thought_frame_fixture::write(address,expected,target,count);}}
struct Port{
    Runtime& r;const a::Context& context;const a::Binding& binding;Bound origin;live::ManagementReceipt& receipt;std::set<uintptr_t> retained;
    bool current(std::string& e){return world_valid||fail(e,"fixture fresh World changed");}
    bool call(Call& work,std::string& e){if(!current(e))return false;receipt.attempted=true;++receipt.attempted_stores;const auto exception=invoke(work);if(exception)return fail(e,"fixture original operation failed; no replay");++receipt.stores;if(work.op==Op::Allocate||work.op==Op::AllocateArray)retained.insert(work.result);if(work.op==Op::TimeDestroy||work.op==Op::FreeArray||work.op==Op::ThoughtDestroy)retained.erase(work.object);return true;}
    bool scalar(uintptr_t address,bool byte,uint32_t before,uint32_t target,std::string& e){if(!current(e))return false;receipt.attempted=true;++receipt.attempted_stores;uint32_t exception=0;if(byte){const auto prior=uint8_t(before),after=uint8_t(target);exception=write(address,&prior,&after,1);}else exception=write(address,&before,&target,4);if(exception)return fail(e,"fixture scalar prior/store failed; no replay");++receipt.stores;return true;}
    bool pointer(uintptr_t address,uintptr_t before,uintptr_t target,std::string& e){if(!current(e))return false;receipt.attempted=true;++receipt.attempted_stores;const auto exception=write(address,&before,&target,8);if(exception)return fail(e,"fixture pointer prior/store failed; no replay");++receipt.stores;return true;}
    bool observed(const values::State& expected,Snapshot& out,std::string& e){if(!sample(r,context,binding,out,e))return false;return (out.bound==origin&&out.value==expected)||fail(e,"fixture complete actual state changed");}
    bool row(uintptr_t thinker,uintptr_t object,values::LocalRow& owner,values::Row& value,std::string& e){if(!current(e))return false;std::array<std::map<uintptr_t,std::string>,3> keys;for(size_t i=0;i<2;++i)for(const auto& item:origin.bindings.targets[i])keys[i].emplace(item.second,item.first);for(const auto& item:origin.bindings.toons)keys[2].emplace(item.second,item.first);auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){return at(r,base,offset,out,cause);};return values::read_row(read,image,thinker,object,keys,owner,value,e);}
    bool array(uintptr_t toon,const values::Array& expected,std::string& e){if(!current(e)||memory.get<uintptr_t>(toon+0x8c0)!=image+0x16eaae0)return fail(e,"fixture current ObjectArray class differs");return array_at(toon,0x8c8)==expected||fail(e,"fixture current array header changed");}
};
#include "thought_frame_adopt_actual.inc"
values::State empty(){return {{{"toon:1",{}},{"toon:2",{}}},{{0,"building",{}},{1,"zone",{}}}};}
values::State target(){auto value=empty();values::Row first;first.type=12;first.time={17,3,2,1};first.position={0x3f800000,0x40000000,0x40400000};first.targets[0]="building";first.registered=true;
    auto second=first;second.type=49;second.targets={};second.targets[1]="zone";second.registered=false;auto third=first;third.type=3;third.targets={};third.targets[2]="toon:2";auto fourth=first;fourth.type=0;fourth.targets={};fourth.registered=false;
    value.toons[0].thoughts={first,second,third,fourth};value.toons[1].thoughts={first};value.links[0].members={"toon:2/thought/0","toon:1/thought/0"};value.links[1].members={"toon:1/thought/1"};return value;}
void reset(){memory=Memory{};calls=writes=fail_call=fail_write=0;world_valid=true;drift=false;registrations.clear();constructed.clear();last=nullptr;
    for(const auto toon:{toon_a,toon_b}){memory.put(toon,image+0x1744e90);memory.put(toon+0x8a8,uintptr_t{0x1234});memory.put(toon+0x8c0,image+0x16eaae0);memory.put(toon+0x8c8,uintptr_t{0});memory.put(toon+0x8d0,std::array<int32_t,2>{0,0});}
    for(const auto& [object,base]:std::array<std::pair<uintptr_t,size_t>,2>{{{building,0xce0},{zone,0x580}}}){memory.put(object+base,image+0x1717220);const auto data=memory.allocate(128);memory.put(object+base+0x20,data);memory.put(object+base+0x28,std::array<int32_t,2>{0,16});}}
void run(){Runtime runtime;a::Context context;context.role=a::Role::Client;a::Binding binding;live::ManagementReceipt receipt;std::string e;Snapshot actual;const auto desired=target();
    reset();need(adopt(runtime,context,binding,desired,receipt,e)&&last.at("confirmed")==true,"actual complete Thought adoption: "+e);need(sample(runtime,context,binding,actual,e)&&actual.value==desired,"actual full Thought owner/value/reverse order readback: "+e);const auto successful_calls=calls,successful_writes=writes;
    need(constructed.size()==5&&registrations[12]==2&&registrations[49]==0&&registrations[3]==1,"Thought original constructors/statistics bookkeeping differ");need(memory.get<uintptr_t>(toon_a+0x8a8)==0x1234,"Thought projection touched prior memory array");
    receipt={};const auto prior_calls=calls,prior_writes=writes;need(adopt(runtime,context,binding,desired,receipt,e)&&!receipt.attempted&&calls==prior_calls&&writes==prior_writes,"unchanged Thoughts repeated original effects");
    receipt={};need(adopt(runtime,context,binding,empty(),receipt,e)&&sample(runtime,context,binding,actual,e)&&actual.value==empty()&&registrations[12]==0&&registrations[3]==0,"original Thought retirement did not remove reciprocal/statistics owners: "+e);
    for(size_t index=1;index<=successful_calls;++index){reset();fail_call=index;receipt={};need(!adopt(runtime,context,binding,desired,receipt,e)&&receipt.attempted&&last.at("unknown")==true&&!e.empty()&&calls==index,"failed original Thought call replayed/continued or hid its cause");}
    for(size_t index=1;index<=successful_writes;++index){reset();fail_write=index;receipt={};need(!adopt(runtime,context,binding,desired,receipt,e)&&receipt.attempted&&last.at("unknown")==true&&!e.empty()&&writes==index,"failed Thought ownership publication continued or hid cause");}
    reset();drift=true;receipt={};need(!adopt(runtime,context,binding,desired,receipt,e)&&calls==1&&last.at("unknown")==true,"Thought original callback changed World without abort");
    reset();world_valid=false;receipt={};need(!adopt(runtime,context,binding,desired,receipt,e)&&!receipt.attempted&&calls==0,"Thought foreign World was mutated");
    reset();memory.put(toon_a+0x8c0,uintptr_t{0});need(!sample(runtime,context,binding,actual,e),"wrong owning ObjectArray AP accepted");
    reset();const auto data=memory.get<uintptr_t>(zone+0x5a0);memory.put(data,uintptr_t{1});memory.put(zone+0x5a8,int32_t{1});need(!sample(runtime,context,binding,actual,e)&&e.find("outside complete live owning")!=std::string::npos,"foreign reciprocal identity dereferenced or omitted");
    reset();receipt={};need(adopt(runtime,context,binding,desired,receipt,e),"Thought failure fixture seed: "+e);const auto object=memory.get<uintptr_t>(memory.get<uintptr_t>(toon_a+0x8c8));const auto thinker=memory.get<uintptr_t>(object+0x18);memory.put(object+0x18,toon_b);need(!sample(runtime,context,binding,actual,e),"Thought foreign thinker accepted");memory.put(object+0x18,thinker);
    const auto proxy=memory.get<uintptr_t>(object+0x58);memory.put(proxy,zone);need(!sample(runtime,context,binding,actual,e),"Thought weak proxy/target mismatch accepted");memory.put(proxy,building);
    auto wire=values::encode(desired);values::State decoded;need(values::decode(wire,decoded,e)&&decoded==desired,"Thought exact codec roundtrip");wire["toons"][0]["thoughts"][0][0]=50;need(!values::decode(wire,decoded,e)&&decoded==values::State{},"Thought invalid native enum accepted");
    wire=values::encode(desired);wire["toons"][0]["thoughts"][0][2][0]=uint32_t{0x7f800000};need(!values::decode(wire,decoded,e),"Thought nonfinite position accepted");wire=values::encode(desired);wire["links"][0][2].push_back("toon:1/thought/0");need(!values::decode(wire,decoded,e),"duplicate reverse Thought membership accepted");
    auto invalid=desired;invalid.links[0].members.clear();need(!values::valid(invalid,e),"missing reciprocal Thought accepted");invalid=desired;invalid.toons[0].thoughts[0].targets[2]="toon:2";reset();receipt={};need(!adopt(runtime,context,binding,invalid,receipt,e)&&!receipt.attempted,"unsupported multi-target ctor fabricated effects");
}
}
