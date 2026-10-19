namespace {
bool intervention_subscribers(Runtime& r,subscriber_intervention::State& values,subscriber_intervention::Owners& owners,std::string& error){
    if(GetCurrentThreadId()!=r.thread)return fail(error,"Subscriber intervention current values require bound Mainthread");
    auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& cause){return at(r,object,offset,out,cause);};
    auto copy=[&](uintptr_t object,void* out,size_t size,std::string& cause){return r.memory.copy(object,out,size,cause);};
    return ([&]{live::apply_timing::PageReadScope subscriber_pages;return subscriber_intervention::read_all(read,copy,r.image,values,owners,error);}());
}
bool intervention_adopt_subscribers(Runtime& r,const a::Context& context,const a::Binding& binding,const subscriber_intervention::State& desired,live::ManagementReceipt& receipt,std::string& error){
    live::NativePort port(context,binding,nullptr);live::Pin pin;if(!port.read(pin,error))return false;
    const auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& cause){return at(r,object,offset,out,cause);};
    auto sample=[&](auto& values,auto& owners,auto& cause){return intervention_subscribers(r,values,owners,cause);};
    auto guard=[&](const subscriber_intervention::Owners& owners,size_t n,std::string& cause){uintptr_t manager=0,data=0,member=0;int32_t count=-1,capacity=-1;const auto& [object,row]=owners.rows[n];
        if(!port.current_owners(pin,cause)||!at(r,r.image,0x13df690,manager,cause)||!at(r,manager,0x40,data,cause)||!at(r,manager,0x48,count,cause)||!at(r,manager,0x4c,capacity,cause)||manager!=owners.manager||data!=owners.data||count!=owners.count||capacity!=owners.capacity||!at(r,data,n*8,member,cause)||member!=object)return fail(cause,cause.empty()?"Subscriber intervention current physical owner changed before value adoption":cause);
        subscriber_intervention::Value actual;if(!subscriber_intervention::read(read,r.image,object,row.id,actual,cause)||actual!=row.value)return fail(cause,cause.empty()?"Subscriber intervention current value changed before authoritative CAS":cause);return true;};
    auto compare=[&](uintptr_t address,unsigned size,uint32_t old,uint32_t value,std::string& cause){bool exchanged=false;const auto exception=gm_ticket::native::compare(address,size,old,value,exchanged);
        return (!exception&&exchanged)||fail(cause,"Subscriber intervention authoritative native CAS failed SEH="+std::to_string(exception));};
    if(!subscriber_intervention::adopt(desired,sample,guard,compare,receipt,error))return false;
    return port.current_owners(pin,error);
}
}
