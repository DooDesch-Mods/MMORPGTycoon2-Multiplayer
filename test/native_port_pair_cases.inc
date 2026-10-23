namespace native_port_pair_fixture {
namespace apply_timing=t;
struct Context {std::string session="session",snapshot_uuid="snapshot";bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=1,native_generation=2;bool operator==(const Binding&)const=default;};
struct Values {std::array<int32_t,6> calendar{};int32_t cash=0;std::array<int32_t,5> record{};std::array<std::array<int32_t,2>,20> lines{};bool operator==(const Values&)const=default;};
struct Pin {Context context;Binding binding;uintptr_t image=0,clock=0,business=0,record=0,history=0;int32_t history_count=0,history_capacity=0;Values values;float selected=0,pause=0;bool operator==(const Pin&)const=default;};
struct Frame {std::string session,snapshot;uint64_t epoch=0,source_generation=0,revision=0,time_revision=0;Values values;float selected=0,pause=0;};
namespace action_session {struct Ledger {int32_t cash=0,day=0,present=1,record_day=0,record_cash=0,income=0,expense=0;std::array<int32_t,20> counts{},lines{};bool operator==(const Ledger&)const=default;};}
action_session::Ledger ledger(const Values&){return {};}
namespace native {
struct WorldStatus {int32_t saves_in_progress=0,edit_operations=0;};
struct SimulationState {int32_t cash=0;struct {int32_t flat_day=0;} clock;bool record_present=true;int32_t record_day=0,record_cash=0,record_income=0,record_expense=0;std::array<int32_t,20> line_counts{},line_income{};};
}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Fixture {
    void* page=nullptr;std::map<uintptr_t,std::vector<unsigned char>> fields;
    std::vector<std::pair<uintptr_t,size_t>> reads;
    size_t passes=0,simulations=0,fail_read=0;int scenario=0;
    template<class T>void put(uintptr_t address,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);fields[address]={bytes,bytes+sizeof value};}
    explicit Fixture(void* owned,int failure):page(owned),scenario(failure){
        constexpr uintptr_t image=0x10000000,clock=0x20000000,business=0x30000000,record=0x40000000,history=0x50000000;
        put(image+0x13df9a0,clock);put(clock,image+0x17499d0);put(image+0x13df220,business);put(business,image+0x16d97e0);
        put(business+0x100,record);put(record,image+0x1712c00);put(record+0x18,int32_t(2));put(business+0x60,image+0x16ec280);
        put(business+0x68,history);put(business+0x70,int32_t(1));put(business+0x74,int32_t(1));put(history,record);
        for(size_t i=0;i<6;++i)put(clock+0x38+i*4,int32_t(0));put(clock+0x58,0.0f);put(clock+0x5c,0.0f);put(business+0x38,int32_t(0));
        for(size_t i=0;i<5;++i)put(record+0x1c+i*4,int32_t(0));
        for(size_t i=0;i<20;++i){const auto line=record+0x30+i*0x48;put(line,image+0x171d0d0);put(line+0x20,int32_t(i));put(line+0x18,int32_t(0));put(line+0x1c,int32_t(0));}
        if(scenario==5)put(history,uintptr_t(0));
        if(scenario==6)put(record+0x18,int32_t(1));
        if(scenario==7)put(record+0x30+0x20,int32_t(8));
        if(scenario==8)fail_read=12;
    }
};
Fixture* fixture=nullptr;
template<class T>bool at(uintptr_t base,size_t offset,T& value,std::string& error){
    auto& f=*fixture;const auto address=base+offset;f.reads.emplace_back(address,sizeof value);
    if(f.fail_read==f.reads.size())return fail(error,"original native byte read failed");
    MEMORY_BASIC_INFORMATION info{};if(!t::virtual_query(f.page,&info,sizeof info,t::Part::QueryPlacement)||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return fail(error,"original native page unreadable");
    const auto found=f.fields.find(address);if(found==f.fields.end()||found->second.size()!=sizeof value)return fail(error,"fixture field absent");
    std::memcpy(&value,found->second.data(),sizeof value);return true;
}
bool ap(uintptr_t object,uintptr_t image,uintptr_t expected,std::string& error){uintptr_t actual=0;return at(object,0,actual,error)&&(actual==image+expected||fail(error,"live native class addresspoint differs"));}
bool current_scope(const Context& context,const Binding& binding,Pin& out,native::WorldStatus& status,std::string& error){
    auto& f=*fixture;++f.passes;out={};out.context=context;out.binding=binding;out.image=0x10000000;
    if(f.scenario==1&&f.passes==2)return fail(error,"original current world differs");
    if(f.scenario==2&&f.passes==2)f.put(uintptr_t(0x20000058),1.0f);
    if(f.scenario==3)status.saves_in_progress=1;
    return true;
}
namespace native {bool read_simulation_state(SimulationState& out,std::string& error){++fixture->simulations;if(fixture->scenario==4)return fail(error,"original independent reader failed");if(fixture->scenario==9)out.cash=1;return true;}}
bool valid(const Frame&,std::string& error){return fixture->scenario!=10||fail(error,"original frame validation failed");}
#include "native_port_activity.inc"
struct NativePort {Context context_;Binding binding_;bool read_actual(Pin&,std::string&);bool read_prior(Pin&,std::string&);};
#include "native_port_pair_actual.inc"
#include "native_port_pair_prior.inc"
void checks(){
    void* page=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);need(page!=nullptr,"allocate NativePort pair fixture page");
    try{
        const auto run=[&](bool prior,int scenario){Fixture state(page,scenario);fixture=&state;NativePort port;Pin out;std::string error="caller cause";
            const auto queries=count(t::Part::MemoryVirtualQuery);const bool okay=prior?port.read_prior(out,error):port.read_actual(out,error);
            return std::tuple{okay,error,out,state.reads,state.passes,state.simulations,count(t::Part::MemoryVirtualQuery)-queries};};
        t::Scope take(t::Part::Take);
        for(int scenario=0;scenario<=10;++scenario){const auto before=run(true,scenario),after=run(false,scenario);
            need(std::get<0>(before)==std::get<0>(after)&&std::get<1>(before)==std::get<1>(after)&&std::get<2>(before)==std::get<2>(after)&&std::get<3>(before)==std::get<3>(after)&&std::get<4>(before)==std::get<4>(after)&&std::get<5>(before)==std::get<5>(after),"NativePort pair changed bytes, owners, original failure or independent reads");
            if(!scenario){need(std::get<0>(after)&&std::get<4>(after)==2&&std::get<5>(after)==2,"both complete NativePort passes must execute");need(std::get<6>(before)==2&&std::get<6>(after)==1,"NativePort pair must halve repeated permission queries");}
            else need(!std::get<0>(after),"NativePort negative fixture unexpectedly accepted");
        }
        {t::PageReadScope outer;const auto before=count(t::Part::MemoryVirtualQuery);need(std::get<0>(run(false,0))&&std::get<0>(run(false,0))&&count(t::Part::MemoryVirtualQuery)-before==1,"NativePort pair must borrow an existing pure reader");}
        {t::PageReadScope original(t::PageBoundary::OriginalCall);const auto before=count(t::Part::MemoryVirtualQuery);need(std::get<0>(run(false,0))&&std::get<0>(run(false,0))&&count(t::Part::MemoryVirtualQuery)-before==2,"NativePort pair must not borrow an original-call scope");}
        DWORD previous=0;need(VirtualProtect(page,4096,PAGE_NOACCESS,&previous)!=0,"protect NativePort test page");const auto denied=run(false,0);need(!std::get<0>(denied)&&std::get<1>(denied)=="original native page unreadable","next NativePort read must reject changed page permission");
        need(VirtualProtect(page,4096,previous,&previous)!=0,"restore NativePort test page");need(std::get<0>(run(false,0)),"NativePort read must recover with fresh page metadata");
    }catch(...){if(!VirtualFree(page,0,MEM_RELEASE))std::cerr<<"error: NativePort fixture page cleanup failed "<<GetLastError()<<'\n';throw;}
    need(VirtualFree(page,0,MEM_RELEASE)!=0,"release NativePort fixture page");fixture=nullptr;
}
}
