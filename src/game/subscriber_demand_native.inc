namespace subscriber_demand_native {
namespace values=subscriber_demand;
nlohmann::json last;
bool sample(Runtime& r,const a::Context& c,const a::Binding& b,values::Owners& owners,values::State& out,std::map<uintptr_t,std::string>& keys,live::Pin& pin,std::string& e){
    a::Context actual;live::NativePort port(c,b,nullptr);
    if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=c||!port.read(pin,e)||pin.image!=r.image)return fail(e,e.empty()?"Subscriber Demand current Session/Mainthread/image differs":e);
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!character_calendar::demand_keys(c,b,keys,e))return false;}
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
    values::Owners first;values::State before;
    {live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());if(!values::read(read,r.image,keys,first,before,e))return false;}
    std::map<uintptr_t,std::string> repeated;
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!character_calendar::demand_keys(c,b,repeated,e))return false;}
    if(repeated!=keys)return fail(e,"Subscriber Demand registered canonical owners changed during capture");
    {live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());if(!values::read(read,r.image,keys,owners,out,e))return false;}
    if(first!=owners||before!=out||!port.current_owners(pin,e))return fail(e,e.empty()?"Subscriber Demand complete current values/owners changed during double read":e);
    e.clear();return true;
}
uint32_t compare_pointer(uintptr_t address,uintptr_t before,uintptr_t target,bool& stored)noexcept{
    __try{stored=uintptr_t(InterlockedCompareExchange64(reinterpret_cast<volatile LONG64*>(address),LONG64(target),LONG64(before)))==before;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool apply(Runtime& r,const a::Context& c,const a::Binding& b,const values::State& target,const character_calendar::State& calendar,live::ManagementReceipt& receipt,std::string& e){
    if(!values::reciprocal(target,calendar,e))return false;
    values::Owners owners;values::State before;std::map<uintptr_t,std::string> keys;live::Pin pin;
    if(c.role!=a::Role::Client||!sample(r,c,b,owners,before,keys,pin,e)||pin.selected!=0)return fail(e,e.empty()?"Subscriber Demand Apply requires current Client Clock0":e);
    character_calendar::State current;
    {live::apply_timing::PageReadScope boundary(live::apply_timing::PageBoundary::OriginalCall);if(!character_calendar::capture(c,b,current,e))return false;}
    if(current!=calendar)return fail(e,"Subscriber Demand owning Character state has not completed authoritative adoption");
    if(before.size()!=target.size())return fail(e,"Subscriber Demand member census differs after original births/retirements");
    std::map<std::string,uintptr_t> reverse;for(const auto& [pointer,key]:keys)if(!reverse.emplace(key,pointer).second)return fail(e,"Subscriber Demand canonical owner resolves to multiple allocations");
    for(size_t i=0;i<before.size();++i)if(before[i].id!=target[i].id||(target[i].key&&!reverse.contains(*target[i].key)))return fail(e,"Subscriber Demand current ID or requested canonical owner absent");
    last={{"attempted",false},{"confirmed",false},{"unknown",false},{"stores",0}};
    const auto failed=[&](){last["unknown"]=receipt.attempted;last["cause"]=e;return false;};
    auto expected=before;auto expected_owners=owners;
    for(size_t i=0;i<before.size();++i){if(before[i]==target[i])continue;
        live::NativePort port(c,b,nullptr);if(!port.current_owners(pin,e)||!live::economic_fence().read_current_fence(pin,e))return failed();
        values::Owners fresh;values::State actual;std::map<uintptr_t,std::string> current_keys;live::Pin current_pin;
        if(!sample(r,c,b,fresh,actual,current_keys,current_pin,e)||fresh!=expected_owners||actual!=expected||current_keys!=keys||current_pin!=pin){if(e.empty())e="Subscriber Demand fresh CAS prior differs";return failed();}
        const auto pointer=target[i].key?reverse.at(*target[i].key):uintptr_t{0};
        bool stored=false;receipt.attempted=true;++receipt.attempted_stores;last["attempted"]=true;
        const auto exception=compare_pointer(expected_owners.rows[i].object+0xc0,expected_owners.rows[i].demand,pointer,stored);
        if(exception||!stored){e="Subscriber Demand original owner pointer CAS failed SEH="+std::to_string(exception)+"; no replay";return failed();}
        ++receipt.stores;last["stores"]=last["stores"].get<uint64_t>()+1;expected_owners.rows[i].demand=pointer;expected[i]=target[i];
        if(!sample(r,c,b,fresh,actual,current_keys,current_pin,e)||fresh!=expected_owners||actual!=expected||current_keys!=keys||current_pin!=pin){if(e.empty())e="Subscriber Demand complete fresh CAS readback differs";return failed();}
    }
    if(!values::reciprocal(expected,current,e))return failed();last["confirmed"]=true;e.clear();return true;
}
}
