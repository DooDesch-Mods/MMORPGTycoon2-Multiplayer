namespace thought_frame_native {
namespace values=toon_thought;
struct Bound {
    live::Pin pin;values::Bindings bindings;uintptr_t manager=0,data=0;int32_t count=0,capacity=0;
    std::vector<std::pair<uintptr_t,int32_t>> subscribers;std::map<uintptr_t,uint64_t> uids;std::vector<world_zone_runtime::Current> zones;
    bool operator==(const Bound&)const=default;
};
struct Snapshot {Bound bound;values::Owners owners;values::State value;};
nlohmann::json last;
bool bind(Runtime& r,const a::Context& c,const a::Binding& b,Bound& out,std::string& e){
    out={};a::Context current;live::NativePort port(c,b,nullptr);
    if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(current,e)||current!=c||!port.read(out.pin,e)||out.pin.image!=r.image)return fail(e,e.empty()?"Thought current Frame Session/Mainthread/image differs":e);
    if(!at(r,r.image,0x13df690,out.manager,e)||!ap(r,out.manager,0x1725f40,e)||!at(r,out.manager,0x40,out.data,e)||!at(r,out.manager,0x48,out.count,e)||!at(r,out.manager,0x4c,out.capacity,e)||out.count<0||out.count>1000000||out.capacity<out.count||out.capacity>1000000||(!out.data&&out.capacity))return fail(e,e.empty()?"Thought current complete Subscriber manager extent differs":e);
    std::set<uintptr_t> members;std::set<int32_t> ids;
    for(int32_t i=0;i<out.count;++i){uintptr_t subscriber=0,toon=0,reverse=0;int32_t id=-1;
        if(!at(r,out.data,size_t(i)*8,subscriber,e)||!ap(r,subscriber,0x16e75f0,e)||!at(r,subscriber,0x20,id,e)||id<0||!at(r,subscriber,0x80,toon,e)||!members.insert(subscriber).second||!ids.insert(id).second)return fail(e,e.empty()?"Thought actual Subscriber class/ID/member differs":e);
        out.subscribers.emplace_back(subscriber,id);if(!toon)continue;
        if(!ap(r,toon,0x1744e90,e)||!at(r,toon,0x940,reverse,e)||reverse!=subscriber||!out.bindings.toons.emplace("toon:"+std::to_string(id),toon).second)return fail(e,e.empty()?"Thought current Toon reciprocal Subscriber/class differs":e);
    }
    std::vector<remove_diagnostics::LocalTarget> buildings;
    {std::optional<live::apply_timing::Scope> measured_identity;if(live::apply_timing::host_capture_depth)measured_identity.emplace(live::apply_timing::host_query_part==live::apply_timing::Part::HostQueryManagementFinish?live::apply_timing::Part::HostManagementFinishIdentityBuildings:live::apply_timing::Part::HostManagementIdentityBuildings);live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!remove_diagnostics::identity_building_targets(buildings,e))return false;}
    for(const auto& item:buildings)if(item.normal_building){if(item.world_epoch!=b.native_epoch||item.world_generation!=b.native_generation||!ap(r,item.object,0x16d93c0,e)||!out.bindings.targets[0].emplace(item.record_key,item.object).second)return fail(e,e.empty()?"Thought Building current canonical key/World differs":e);}
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!world_zone_runtime::read_current(b,c.snapshot_uuid,out.zones,e))return false;}
    std::sort(out.zones.begin(),out.zones.end(),[](const auto& x,const auto& y){return x.key<y.key;});
    for(const auto& zone:out.zones)if(!ap(r,zone.object,0x1745ce0,e)||!out.bindings.targets[1].emplace(zone.key,zone.object).second)return fail(e,e.empty()?"Thought Zone current canonical key/class differs":e);
    for(const auto* objects:{&out.bindings.toons,&out.bindings.targets[0],&out.bindings.targets[1]})for(const auto& item:*objects){uint64_t uid=0;uintptr_t registered=0;
        if(!at(r,item.second,0x18,uid,e)||!uid||!out.uids.emplace(item.second,uid).second)return fail(e,e.empty()?"Thought canonical allocation/full UID repeats":e);
        uint32_t exception=0;{live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);exception=uid_lookup(r.image,uid,registered);}
        if(exception||registered!=item.second)return fail(e,"Thought current original full UID lookup differs exception="+std::to_string(exception));
    }
    return port.current_owners(out.pin,e);
}
bool once(Runtime& r,const a::Context& c,const a::Binding& b,Snapshot& out,std::string& e){
    out={};if(!bind(r,c,b,out.bound,e))return false;auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    {live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());if(!values::read(read,r.image,out.bound.bindings,out.owners,out.value,e))return false;}return true;
}
bool sample(Runtime& r,const a::Context& c,const a::Binding& b,Snapshot& out,std::string& e){Snapshot first;if(!once(r,c,b,first,e)||!once(r,c,b,out,e))return false;
    return (first.bound==out.bound&&first.owners==out.owners&&first.value==out.value)||fail(e,"Thought complete current owners/values changed during double read");}
bool code(Runtime& r,std::string& e){struct Body{uintptr_t rva;size_t bytes;const char* sha;};constexpr Body bodies[]={
#include "toon_thought_codes.inc"
    };for(const auto& body:bodies){std::vector<unsigned char> bytes(body.bytes);if(!r.memory.copy(r.image+body.rva,bytes.data(),bytes.size(),e)||mt2mp::sha256(bytes)!=body.sha)return fail(e,e.empty()?"Thought original constructor/bookkeeping body differs RVA="+std::to_string(body.rva):e);}
    if(!thought_native::enabled||!thought_native::fault.empty())return fail(e,thought_native::fault.empty()?"Thought lifetime bridge is not enabled":thought_native::fault);
    auto copy=[&](auto p,auto out,auto count,auto& cause){return r.memory.copy(p,out,count,cause);};auto hash=[](const auto& bytes){return mt2mp::sha256(bytes);};
    return release_ui_owner::verify(thought_native::destroy_owner,copy,hash,"b809f7293fa4d0c143cddd610b18480853bc841ab4ea3441fc6aacd40b0dc625",e);
}
enum class Op{Allocate,AllocateArray,FreeArray,Time,TimeDestroy,Thought,Clear,Notify,Register,Deregister,ThoughtDestroy};
struct Call {Op op=Op::Allocate;uintptr_t image=0,object=0,owner=0,target=0,time=0,result=0;size_t bytes=0;int32_t kind=-1;const values::Row* value=nullptr;};
uint32_t invoke(Call& call)noexcept{__try{switch(call.op){
    case Op::Allocate:call.result=reinterpret_cast<uintptr_t>(reinterpret_cast<void*(*)(size_t)>(call.image+0xa02670)(call.bytes));break;
    case Op::AllocateArray:call.result=reinterpret_cast<uintptr_t>(reinterpret_cast<void*(*)(size_t)>(call.image+0xa026c0)(call.bytes));break;
    case Op::FreeArray:reinterpret_cast<void(*)(void*)>(call.image+0xa01280)(reinterpret_cast<void*>(call.object));break;
    case Op::Time:reinterpret_cast<void(*)(void*,int32_t,int32_t,int32_t,int32_t)>(call.image+0x88ef80)(reinterpret_cast<void*>(call.object),call.value->time[0],call.value->time[1],call.value->time[2],call.value->time[3]);break;
    case Op::TimeDestroy:reinterpret_cast<void(*)(void*)>(call.image+0xc36ba0)(reinterpret_cast<void*>(call.object));break;
    case Op::Thought:if(call.kind<0)reinterpret_cast<void(*)(void*,int32_t,void*,const void*)>(call.image+0x3f1260)(reinterpret_cast<void*>(call.object),call.value->type,reinterpret_cast<void*>(call.owner),reinterpret_cast<void*>(call.time));
        else reinterpret_cast<void(*)(void*,int32_t,void*,void*,const void*)>(call.image+(call.kind==0?0x3f0db0:call.kind==1?0x3f15a0:0x3f17d0))(reinterpret_cast<void*>(call.object),call.value->type,reinterpret_cast<void*>(call.owner),reinterpret_cast<void*>(call.target),reinterpret_cast<void*>(call.time));break;
    case Op::Clear:reinterpret_cast<void(*)(void*)>(call.image+0x3dfb90)(reinterpret_cast<void*>(call.object));break;
    case Op::Notify:reinterpret_cast<void(*)(void*,void*)>(call.image+(call.kind==0?0x4050c0:0x451f60))(reinterpret_cast<void*>(call.target),reinterpret_cast<void*>(call.object));break;
    case Op::Register:case Op::Deregister:{auto* statistics=reinterpret_cast<void*(*)()>(call.image+0xb32220)();reinterpret_cast<void(*)(void*,void*)>(call.image+(call.op==Op::Register?0x3f7550:0x3f7ab0))(statistics,reinterpret_cast<void*>(call.object));break;}
    case Op::ThoughtDestroy:reinterpret_cast<void(*)(void*)>(call.image+0x3f1170)(reinterpret_cast<void*>(call.object));break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct Port {
    Runtime& r;const a::Context& context;const a::Binding& binding;Bound origin;live::ManagementReceipt& receipt;
    std::set<uintptr_t> retained;
    bool current(std::string& e){Bound fresh;if(!bind(r,context,binding,fresh,e))return false;return fresh==origin||fail(e,"Thought Apply complete current physical binding/Clock/Ledger changed");}
    bool call(Call& work,std::string& e){if(!current(e))return false;work.image=r.image;receipt.attempted=true;++receipt.attempted_stores;uint32_t exception=0;{live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);exception=invoke(work);}if(exception)return fail(e,"Thought original call failed operation="+std::to_string(int(work.op))+" exception="+std::to_string(exception)+"; no replay");++receipt.stores;
        if(work.op==Op::Allocate||work.op==Op::AllocateArray){if(!work.result)return fail(e,"Thought original allocation returned null");retained.insert(work.result);}if(work.op==Op::TimeDestroy||work.op==Op::FreeArray||work.op==Op::ThoughtDestroy)retained.erase(work.object);return true;}
    bool scalar(uintptr_t address,bool byte,uint32_t before,uint32_t target,std::string& e){if(!current(e))return false;bool stored=false;receipt.attempted=true;++receipt.attempted_stores;const auto exception=scalar_cas(address,byte,std::bit_cast<int32_t>(before),std::bit_cast<int32_t>(target),stored);if(exception||!stored)return fail(e,"Thought scalar CAS failed exception="+std::to_string(exception)+"; no replay");++receipt.stores;return true;}
    bool pointer(uintptr_t address,uintptr_t before,uintptr_t target,std::string& e){if(!current(e))return false;bool stored=false;receipt.attempted=true;++receipt.attempted_stores;const auto exception=subscriber_demand_native::compare_pointer(address,before,target,stored);if(exception||!stored)return fail(e,"Thought pointer CAS failed exception="+std::to_string(exception)+"; no replay");++receipt.stores;return true;}
    bool observed(const values::State& expected,Snapshot& out,std::string& e){if(!sample(r,context,binding,out,e))return false;return (out.bound==origin&&out.value==expected)||fail(e,"Thought complete original values/readback differs");}
    bool row(uintptr_t thinker,uintptr_t object,values::LocalRow& owner,values::Row& value,std::string& e){if(!current(e))return false;std::array<std::map<uintptr_t,std::string>,3> keys;
        for(size_t i=0;i<2;++i)for(const auto& item:origin.bindings.targets[i])keys[i].emplace(item.second,item.first);for(const auto& item:origin.bindings.toons)keys[2].emplace(item.second,item.first);
        auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){return at(r,base,offset,out,cause);};live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());return values::read_row(read,r.image,thinker,object,keys,owner,value,e);}
    bool array(uintptr_t toon,const values::Array& expected,std::string& e){if(!current(e)||!ap(r,toon+0x8c0,0x16eaae0,e))return false;values::Array actual;auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){return at(r,base,offset,out,cause);};return values::array(read,toon,0x8c8,actual,e)&&(actual==expected||fail(e,"Thought fresh owning array header changed"));}
};
inline bool compatible(const values::State& before,const values::State& target,const Bound& bound,std::string& e){
    if(!values::valid(before,e)||!values::valid(target,e)||before.toons.size()!=target.toons.size()||before.links.size()!=target.links.size())return fail(e,e.empty()?"Thought complete canonical census differs after original births/retirements":e);
    for(size_t i=0;i<before.toons.size();++i){if(before.toons[i].key!=target.toons[i].key)return fail(e,"Thought current canonical Toon order differs");
        for(const auto& row:target.toons[i].thoughts){size_t targets=0;for(size_t kind=0;kind<3;++kind)if(row.targets[kind]){++targets;const auto& objects=kind==2?bound.bindings.toons:bound.bindings.targets[kind];if(!objects.contains(*row.targets[kind]))return fail(e,"Thought canonical native target is absent");}if(targets>1)return fail(e,"Thought target state does not match any original single-target constructor");}}
    for(size_t i=0;i<before.links.size();++i)if(before.links[i].kind!=target.links[i].kind||before.links[i].key!=target.links[i].key)return fail(e,"Thought complete reciprocal canonical target order differs");e.clear();return true;
}
bool resize(Port& p,uintptr_t toon,values::Array& array,size_t amount,std::string& e){
    if(array.count||amount>1000000)return fail(e,"Thought array resize requires original empty owner and bounded target");if(amount<=size_t(array.capacity))return true;
    Call allocation;allocation.op=Op::AllocateArray;allocation.bytes=amount*8;if(!p.array(toon,array,e)||!p.call(allocation,e))return false;
    std::vector<uintptr_t> zeros(amount);p.receipt.attempted=true;++p.receipt.attempted_stores;const auto exception=analytics_native::write(allocation.result,nullptr,zeros.data(),zeros.size()*8);if(exception)return fail(e,"Thought detached array initialization failed exception="+std::to_string(exception));++p.receipt.stores;
    if(!p.array(toon,array,e)||!p.pointer(toon+0x8c8,array.data,allocation.result,e))return false;const auto old=array.data;if(old)p.retained.insert(old);array.data=allocation.result;
    if(!p.array(toon,array,e)||!p.scalar(toon+0x8d4,false,uint32_t(array.capacity),uint32_t(amount),e))return false;array.capacity=int32_t(amount);
    if(!p.array(toon,array,e))return false;p.retained.erase(allocation.result);
    if(old){Call release;release.op=Op::FreeArray;release.object=old;if(!p.call(release,e))return false;}return true;
}
bool construct(Port& p,uintptr_t toon,const values::Row& target,uintptr_t& object,std::string& e){
    object=0;Call timer;timer.op=Op::Allocate;timer.bytes=0x28;if(!p.call(timer,e))return false;Call time_construct;time_construct.op=Op::Time;time_construct.object=timer.result;time_construct.value=&target;if(!p.call(time_construct,e))return false;
    Call allocation;allocation.op=Op::Allocate;allocation.bytes=0x108;if(!p.call(allocation,e))return false;Call create;create.op=Op::Thought;create.object=allocation.result;create.owner=toon;create.time=timer.result;create.value=&target;
    for(size_t kind=0;kind<3;++kind)if(target.targets[kind]){create.kind=int32_t(kind);const auto& objects=kind==2?p.origin.bindings.toons:p.origin.bindings.targets[kind];create.target=objects.at(*target.targets[kind]);}
    if(!p.call(create,e))return false;Call release;release.op=Op::TimeDestroy;release.object=timer.result;if(!p.call(release,e))return false;
    values::LocalRow owner;values::Row actual;if(!p.row(toon,allocation.result,owner,actual,e))return false;
    if(actual.type!=target.type||actual.time!=target.time||actual.targets!=target.targets)return fail(e,"Thought original constructor type/time/canonical target readback differs");
    for(size_t i=0;i<3;++i)if(actual.position[i]!=target.position[i]){values::LocalRow fresh_owner;values::Row fresh;if(!p.row(toon,allocation.result,fresh_owner,fresh,e)||fresh_owner!=owner||fresh!=actual)return fail(e,e.empty()?"Thought detached position prior changed":e);
        if(!p.scalar(allocation.result+0xf8+i*4,false,actual.position[i],target.position[i],e))return false;actual.position[i]=target.position[i];}
    if(actual.registered!=target.registered){values::LocalRow fresh_owner;values::Row fresh;if(!p.row(toon,allocation.result,fresh_owner,fresh,e)||fresh_owner!=owner||fresh!=actual)return fail(e,e.empty()?"Thought detached registration prior changed":e);
        Call registration;registration.op=target.registered?Op::Register:Op::Deregister;registration.object=allocation.result;if(!p.call(registration,e)||!p.scalar(allocation.result+0x104,true,uint32_t(actual.registered),uint32_t(target.registered),e))return false;actual.registered=target.registered;}
    values::LocalRow final_owner;values::Row final_value;if(!p.row(toon,allocation.result,final_owner,final_value,e)||final_owner!=owner||final_value!=target)return fail(e,e.empty()?"Thought detached complete native value readback differs":e);
    object=allocation.result;return true;
}
void remove_expected(values::State& expected,size_t toon){const auto prefix=expected.toons[toon].key+"/thought/";expected.toons[toon].thoughts.clear();for(auto& list:expected.links)std::erase_if(list.members,[&](const std::string& key){return key.starts_with(prefix);});}
bool append_expected(values::State& expected,size_t toon,const values::Row& value,std::string& e){const auto key=values::thought_key(expected.toons[toon].key,expected.toons[toon].thoughts.size());expected.toons[toon].thoughts.push_back(value);
    for(size_t kind=0;kind<2;++kind)if(value.targets[kind]){auto found=std::find_if(expected.links.begin(),expected.links.end(),[&](const auto& list){return list.kind==int32_t(kind)&&list.key==*value.targets[kind];});if(found==expected.links.end())return fail(e,"Thought complete expected reciprocal target is absent");found->members.push_back(key);}return true;}
bool append(Port& p,uintptr_t toon,values::Array& array,const values::Row& target,uintptr_t object,std::string& e){
    if(array.count<0||array.count>=array.capacity||!p.array(toon,array,e))return fail(e,e.empty()?"Thought append original capacity is absent":e);
    values::LocalRow row_owner;values::Row actual;if(!p.row(toon,object,row_owner,actual,e)||actual!=target)return fail(e,e.empty()?"Thought append detached actual value differs":e);
    for(size_t kind=0;kind<2;++kind)if(target.targets[kind]){Call notify;notify.op=Op::Notify;notify.kind=int32_t(kind);notify.object=object;notify.target=p.origin.bindings.targets[kind].at(*target.targets[kind]);if(!p.call(notify,e))return false;}
    uintptr_t prior=0;const auto slot=array.data+size_t(array.count)*8;if(!p.array(toon,array,e)||!at(p.r,slot,0,prior,e)||!p.pointer(slot,prior,object,e))return false;
    uintptr_t slot_value=0;if(!p.array(toon,array,e)||!at(p.r,slot,0,slot_value,e)||slot_value!=object||!p.scalar(toon+0x8d0,false,uint32_t(array.count),uint32_t(array.count+1),e))return fail(e,e.empty()?"Thought owning slot/count publication differs":e);
    ++array.count;p.retained.erase(object);return true;
}
bool reorder(Port& p,const values::State& target,Snapshot& actual,std::string& e){
    std::map<std::string,uintptr_t> objects;for(size_t i=0;i<actual.value.toons.size();++i)for(size_t n=0;n<actual.owners.toons[i].thoughts.size();++n)objects.emplace(values::thought_key(actual.value.toons[i].key,n),actual.owners.toons[i].thoughts[n].object);
    auto expected=actual.value;
    for(size_t i=0;i<target.links.size();++i){const auto& desired=target.links[i];const auto& before=actual.value.links[i];if(desired.members==before.members)continue;
        if(std::set<std::string>(desired.members.begin(),desired.members.end())!=std::set<std::string>(before.members.begin(),before.members.end()))return fail(e,"Thought reciprocal order differs in membership");
        const auto object=p.origin.bindings.targets[size_t(desired.kind)].at(desired.key);const size_t base=desired.kind?0x580:0xce0;auto extent=actual.owners.links[i];
        auto header=[&](){uintptr_t ap_=0;values::Array fresh;auto read=[&](uintptr_t pointer,size_t offset,auto& value,std::string& cause){return at(p.r,pointer,offset,value,cause);};return p.current(e)&&at(p.r,object,base,ap_,e)&&ap_==p.r.image+0x1717220&&values::array(read,object,base+0x20,fresh,e)&&(fresh==extent||fail(e,"Thought reciprocal current native array header changed"));};
        if(!header()||!p.scalar(object+base+0x28,false,uint32_t(extent.count),0,e))return false;extent.count=0;
        std::vector<uintptr_t> slots;for(const auto& key:before.members)slots.push_back(objects.at(key));
        for(size_t n=0;n<desired.members.size();++n){if(!header())return false;for(size_t index=0;index<slots.size();++index){uintptr_t value=0;if(!at(p.r,extent.data,index*8,value,e)||value!=slots[index])return fail(e,e.empty()?"Thought reciprocal complete slot prior changed":e);}
            const auto pointer=objects.at(desired.members[n]);if(pointer!=slots[n]&&!p.pointer(extent.data+n*8,slots[n],pointer,e))return false;slots[n]=pointer;}
        if(!header()||!p.scalar(object+base+0x28,false,0,uint32_t(desired.members.size()),e))return false;expected.links[i]=desired;
        Snapshot checked;if(!p.observed(expected,checked,e))return false;actual=std::move(checked);
    }
    return true;
}
bool adopt(Runtime& r,const a::Context& c,const a::Binding& b,const values::State& target,live::ManagementReceipt& receipt,std::string& e){
    Snapshot before;if(c.role!=a::Role::Client||!sample(r,c,b,before,e)||before.bound.pin.selected!=0||!compatible(before.value,target,before.bound,e)||!code(r,e))return fail(e,e.empty()?"Thought Apply requires complete current Client Clock0":e);
    Port port{r,c,b,before.bound,receipt,{}};last={{"attempted",false},{"confirmed",false},{"unknown",false},{"retained",nlohmann::json::array()}};
    auto failed=[&](){last["attempted"]=receipt.attempted;last["unknown"]=receipt.attempted;last["cause"]=e;last["retained"]=port.retained;return false;};
    try{
    if(before.value==target){last["confirmed"]=true;e.clear();return true;}
    auto expected=before.value;Snapshot current=before;
    for(size_t i=0;i<target.toons.size();++i){if(expected.toons[i]==target.toons[i])continue;
        if(!port.observed(expected,current,e))return failed();const auto toon=port.origin.bindings.toons.at(target.toons[i].key);
        Call clear;clear.op=Op::Clear;clear.object=toon;if(!port.call(clear,e))return failed();remove_expected(expected,i);if(!port.observed(expected,current,e))return failed();
        auto array=current.owners.toons[i].array;if(!resize(port,toon,array,target.toons[i].thoughts.size(),e))return failed();
        for(const auto& row:target.toons[i].thoughts){if(!port.observed(expected,current,e))return failed();uintptr_t object=0;if(!construct(port,toon,row,object,e)||!append(port,toon,array,row,object,e)||!append_expected(expected,i,row,e)||!port.observed(expected,current,e))return failed();}
    }
    if(!reorder(port,target,current,e)||!port.observed(target,current,e))return failed();if(!port.retained.empty()){e="Thought native resources have no complete ownership/release readback; no replay";return failed();}last={{"attempted",receipt.attempted},{"confirmed",true},{"unknown",false},{"retained",nlohmann::json::array()}};e.clear();return true;
    }catch(const std::exception& error){e=std::string("Thought native adoption: ")+error.what()+"; no replay";return failed();}
}
}
