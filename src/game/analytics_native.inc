namespace analytics_native {
namespace values=analytics_state;
nlohmann::json last;
enum class Op {Day,Reason,Allocate,Free,DayDestroy,Assign};
struct Call {Op op;uintptr_t image=0,object=0,source=0,result=0;size_t bytes=0;};
uint32_t invoke(Call& c)noexcept{
    __try{switch(c.op){
    case Op::Day:c.result=uintptr_t(reinterpret_cast<void*(*)(const void*)>(c.image+0xd91650)(nullptr));break;
    case Op::Reason:c.result=uintptr_t(reinterpret_cast<void*(*)(const void*)>(c.image+0xd8ab00)(nullptr));break;
    case Op::Allocate:c.result=uintptr_t(reinterpret_cast<void*(*)(size_t)>(c.image+0xa026c0)(c.bytes));break;
    case Op::Free:reinterpret_cast<void(*)(void*)>(c.image+0xa01280)(reinterpret_cast<void*>(c.object));break;
    case Op::DayDestroy:reinterpret_cast<void(*)(void*)>(c.image+0xc12720)(reinterpret_cast<void*>(c.object));break;
    case Op::Assign:reinterpret_cast<void*(*)(void*,const void*)>(c.image+0x3fd1e0)(reinterpret_cast<void*>(c.object),reinterpret_cast<const void*>(c.source));break;
    }return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t write(uintptr_t address,const void* expected,const void* desired,size_t bytes)noexcept{
    __try{if(expected&&std::memcmp(reinterpret_cast<void*>(address),expected,bytes))return ERROR_INVALID_DATA;
        std::memcpy(reinterpret_cast<void*>(address),desired,bytes);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool sample(Runtime& r,const a::Context& c,const a::Binding& b,values::Owners& owner,values::State& value,live::Pin& pin,std::string& e){
    a::Context actual;building_mixed_identity::opened_file::CurrentWorld world;
    live::NativePort port(c,b,nullptr);
    if(GetCurrentThreadId()!=r.thread||!a::selected_session().current(actual,e)||actual!=c||!port.read(pin,e)||pin.image!=r.image||
       !native_open_runtime::read_current_world(world,e)||world.world_epoch!=b.native_epoch||world.generation!=b.native_generation||uintptr_t(world.world)!=pin.world)
        return fail(e,e.empty()?"Analytics current Session/Mainthread/World differs":e);
    auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& error){return at(r,object,offset,out,error);};
    auto copy=[&](uintptr_t object,void* out,size_t bytes,std::string& error){return r.memory.copy(object,out,bytes,error);};
    values::Owners first;values::State before;
    {live::apply_timing::PageReadScope pages(!live::apply_timing::PageReadScope::reader_active());
        if(!values::read(read,copy,r.image,uintptr_t(world.state),first,before,e)||!values::read(read,copy,r.image,uintptr_t(world.state),owner,value,e))return false;}
    if(first!=owner||before!=value||!port.current_owners(pin,e))return fail(e,e.empty()?"Analytics complete values/owners changed during double read":e);
    e.clear();return true;
}
bool code(Runtime& r,std::string& e){
    const struct {uintptr_t rva;size_t bytes;const char* sha;} pins[]{
        {0xd91650,176,"376d1dfb05134b47e1cc709a2e90f0243ce0732316e31b9095d8591564c97bce"},
        {0xd8ab00,160,"700776c896ee261670d89cc5b3393ba678b458022dd366608a60265dedd95ee6"},
        {0x3fd1e0,1072,"502740c05f8a0b14e8de9853c3836ff320a9e7f04d4b97ee4e1061609af30583"},
        {0xc12720,192,"af116b469493243e6393bbff9097278afd603aaea9d5aa3cb3fcd881f85eb3e2"}
    };
    for(const auto& pin:pins){std::vector<unsigned char> body(pin.bytes);if(!r.memory.copy(r.image+pin.rva,body.data(),body.size(),e)||mt2mp::sha256(body)!=pin.sha)return fail(e,e.empty()?"Analytics exact native body differs RVA="+std::to_string(pin.rva):e);}
    return true;
}
bool adopt(Runtime& r,const a::Context& c,const a::Binding& b,const values::State& desired,live::ManagementReceipt& receipt,std::string& e){
    if(!values::valid(desired,e))return false;
    values::Owners owners;values::State before;live::Pin pin;
    if(c.role!=a::Role::Client||!sample(r,c,b,owners,before,pin,e)||pin.selected!=0)return fail(e,e.empty()?"Analytics Apply requires actual Client Clock0":e);
    if(before==desired)return true;
    if(!code(r,e))return false;
    last={{"attempted",false},{"confirmed",false},{"unknown",false},{"prepared_days",nlohmann::json::array()},{"resources",nlohmann::json::array()},{"history_transferred",false}};
    const auto guard=[&](){live::NativePort port(c,b,nullptr);native::WorldStatus status;
        if(!native::read_world_status(status,e)||status.saves_in_progress||status.edit_operations||!port.current_owners(pin,e)||!live::economic_fence().read_current_fence(pin,e))return fail(e,e.empty()?"Analytics Apply requires current idle native owner/fence":e);
        return true;};
    const auto failed=[&](){last["unknown"]=receipt.attempted;last["cause"]=e;return false;};
    const auto call=[&](Call& native){
        if(!guard())return false;receipt.attempted=true;++receipt.attempted_stores;last["attempted"]=true;
        live::apply_timing::PageReadScope original(live::apply_timing::PageBoundary::OriginalCall);
        const auto exception=invoke(native);if(exception)return fail(e,"Analytics original resource/ownership call SEH="+std::to_string(exception)+" operation="+std::to_string(int(native.op))+"; no replay");
        if(native.result)last["resources"].push_back({{"operation",int(native.op)},{"address",native.result}});
        ++receipt.stores;return true;};
    const auto store=[&](uintptr_t address,const void* expected,const void* target,size_t bytes){
        if(!guard())return false;receipt.attempted=true;++receipt.attempted_stores;last["attempted"]=true;
        const auto exception=write(address,expected,target,bytes);if(exception)return fail(e,"Analytics exact native storage CAS SEH="+std::to_string(exception)+"; no replay");++receipt.stores;return true;};
    const auto pointers=[&](uintptr_t object,size_t offset,const std::vector<uintptr_t>& rows){
        uintptr_t old=0;std::array<int32_t,2> extent{};if(!at(r,object,offset,old,e)||!at(r,object,offset+8,extent,e)||extent[0]||extent[1]<0)return fail(e,e.empty()?"Analytics private factory owning array is not empty":e);
        uintptr_t data=0;if(!rows.empty()){Call allocation{Op::Allocate,r.image};allocation.bytes=rows.size()*8;if(!call(allocation)||!allocation.result)return fail(e,e.empty()?"Analytics original pointer allocation returned null":e);data=allocation.result;if(!store(data,nullptr,rows.data(),rows.size()*8))return false;}
        const std::array<int32_t,2> target{int32_t(rows.size()),int32_t(rows.size())};
        if(!store(object+offset,&old,&data,8)||!store(object+offset+8,extent.data(),target.data(),8))return false;
        if(old){Call release{Op::Free,r.image,old};if(!call(release))return false;}return true;};
    const auto prepare=[&](const values::Day& day,uintptr_t& object){
        Call creation{Op::Day,r.image};if(!call(creation)||!creation.result)return fail(e,e.empty()?"Analytics original Day factory returned null":e);object=creation.result;last["prepared_days"].push_back(object);
        if(!store(object+0x18,nullptr,&day.day,4))return false;std::vector<uintptr_t> reasons;
        for(const auto& row:day.reasons){Call reason{Op::Reason,r.image};if(!call(reason)||!reason.result)return fail(e,e.empty()?"Analytics original Reason factory returned null":e);
            uintptr_t old=0;std::array<int32_t,2> extent{};if(!at(r,reason.result,0x28,old,e)||!at(r,reason.result,0x30,extent,e)||extent[0]||extent[1]!=4)return fail(e,e.empty()?"Analytics original Reason vector initialization differs":e);
            uintptr_t data=0;if(!row.positions.empty()){Call allocation{Op::Allocate,r.image};allocation.bytes=row.positions.size()*12;if(!call(allocation)||!allocation.result)return fail(e,e.empty()?"Analytics original position allocation returned null":e);data=allocation.result;if(!store(data,nullptr,row.positions.data(),row.positions.size()*12))return false;}
            const std::array<int32_t,2> target{int32_t(row.positions.size()),int32_t(row.positions.size())};
            if(!store(reason.result+0x18,nullptr,&row.type,4)||!store(reason.result+0x28,&old,&data,8)||!store(reason.result+0x30,extent.data(),target.data(),8))return false;
            if(old){Call release{Op::Free,r.image,old};if(!call(release))return false;}reasons.push_back(reason.result);
        }return pointers(object,0x28,reasons);};
    if(before.today!=desired.today||before.history!=desired.history){
        uintptr_t today=0;std::vector<uintptr_t> history;
        if(!prepare(desired.today,today))return failed();
        for(const auto& day:desired.history){uintptr_t object=0;if(!prepare(day,object))return failed();history.push_back(object);}
        uintptr_t history_data=0;if(!history.empty()){Call allocation{Op::Allocate,r.image};allocation.bytes=history.size()*8;if(!call(allocation))return failed();if(!allocation.result){e="Analytics original History pointer allocation returned null";return failed();}history_data=allocation.result;if(!store(history_data,nullptr,history.data(),history.size()*8))return failed();}
        values::Owners current;values::State actual;live::Pin fresh;
        if(!sample(r,c,b,current,actual,fresh,e)||current!=owners||actual!=before||fresh!=pin){if(e.empty())e="Analytics prior changed during detached native resource construction";return failed();}
        Call assignment{Op::Assign,r.image,owners.statistics+0x38,today};if(!call(assignment))return failed();
        const std::array<int32_t,2> old_extent{int32_t(before.history.size()),owners.history_capacity},extent{int32_t(history.size()),int32_t(history.size())};
        if(!store(owners.statistics+0x90,&owners.history_data,&history_data,8)||!store(owners.statistics+0x98,old_extent.data(),extent.data(),8))return failed();
        last["history_transferred"]=true;
        for(size_t i=1;i<owners.days.size();++i){Call destroy{Op::DayDestroy,r.image,owners.days[i]};if(!call(destroy))return failed();}
        if(owners.history_data){Call release{Op::Free,r.image,owners.history_data};if(!call(release))return failed();}
        Call destroy{Op::DayDestroy,r.image,today};if(!call(destroy))return failed();
    }
    if(before.counters!=desired.counters&&!store(owners.analytics+0x40,before.counters.data(),desired.counters.data(),sizeof desired.counters))return failed();
    values::Owners current;values::State actual;live::Pin fresh;
    if(!sample(r,c,b,current,actual,fresh,e)||current.analytics!=owners.analytics||current.statistics!=owners.statistics||actual!=desired||fresh!=pin){if(e.empty())e="Analytics complete native value/parent/Clock/Ledger readback differs";return failed();}
    last["confirmed"]=true;last["unknown"]=false;e.clear();return true;
}
}
