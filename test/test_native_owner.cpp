#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace action_session {
enum class Role {Host,Client};
struct Ledger {
    int32_t cash=0,flat_day=0,record_present=0,record_day=0,record_cash=0,record_income=0,record_expense=0;
    std::array<int32_t,20> line_counts{},line_income{};
    bool operator==(const Ledger&)const=default;
};
}
struct Context {action_session::Role role=action_session::Role::Host;uint64_t session=7;bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=11,native_generation=13;bool operator==(const Binding&)const=default;};
#include "owner_types.inc"
namespace apply_timing {
inline unsigned readers=0;
struct PageReadScope {
    bool enabled;
    explicit PageReadScope(bool value=true):enabled(value){if(enabled)++readers;}
    ~PageReadScope(){if(enabled)--readers;}
    static bool reader_active(){return readers!=0;}
};
}
unsigned assertions=0;
void need(bool ok,const std::string& why){++assertions;if(!ok)throw std::runtime_error(why);}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
constexpr uintptr_t image_base=0x10000000,clock_base=0x20000000,business_base=0x30000000,record_base=0x40000000,history_base=0x50000000,subscribers_base=0x60000000,game_base=0x70000000,mode_base=0x71000000;
struct Fixture {
    std::map<uintptr_t,std::vector<unsigned char>> fields;
    std::vector<std::pair<uintptr_t,size_t>> reads;
    uintptr_t fail_address=0,throw_address=0;
    bool active=true,scope_valid=true,clock_thread=true;
    int32_t saves=0,edits=0;
    unsigned simulation_reads=0;
    template<class T> void put(uintptr_t address,const T& value){auto* p=reinterpret_cast<const unsigned char*>(&value);fields[address]={p,p+sizeof value};}
    Fixture(){
        put(image_base+0x13df9a0,clock_base);put(clock_base,image_base+0x17499d0);
        put(image_base+0x13df220,business_base);put(business_base,image_base+0x16d97e0);
        put(business_base+0x100,record_base);put(record_base,image_base+0x1712c00);put(record_base+0x18,int32_t(2));
        put(business_base+0x60,image_base+0x16ec280);put(business_base+0x68,history_base);
        put(business_base+0x70,int32_t(3));put(business_base+0x74,int32_t(5));
        put(history_base,record_base+0x10000);put(history_base+8,record_base);put(history_base+16,record_base+0x20000);
        for(size_t i=0;i<6;++i)put(clock_base+0x38+i*4,int32_t(i+1));
        put(clock_base+0x58,0.f);put(clock_base+0x5c,1.f);put(business_base+0x38,int32_t(97));
        for(size_t i=0;i<5;++i)put(record_base+0x1c+i*4,int32_t(i+21));
        for(size_t i=0;i<20;++i){auto line=record_base+0x30+i*0x48;put(line,image_base+0x171d0d0);put(line+0x20,int32_t(i));put(line+0x18,int32_t(i+31));put(line+0x1c,int32_t(i+71));}
        put(image_base+0x13df690,subscribers_base);put(subscribers_base,image_base+0x1725f40);put(subscribers_base+0x90,int32_t(9));
        put(image_base+0x13df320,mode_base);put(image_base+0x13df8f0,game_base);
        put(game_base,image_base+0x1743740);put(game_base+0x28,mode_base);
        put(mode_base,image_base+0x16e6730);put(mode_base+0x58,uintptr_t(0x72000000));put(mode_base+0x2c4,int32_t(4));put(mode_base+0x304,uint8_t(0));
    }
}* fixture=nullptr;
uint32_t GetCurrentThreadId(){return 17;}
template<class T> bool at(uintptr_t base,size_t offset,T& value,std::string& error){
    const auto address=base+offset;fixture->reads.emplace_back(address,sizeof value);
    if(fixture->throw_address&&address==fixture->throw_address)throw std::runtime_error("original byte reader exception");
    if(fixture->fail_address&&address==fixture->fail_address)return fail(error,"original byte reader failure");
    auto found=fixture->fields.find(address);
    if(found==fixture->fields.end()||found->second.size()!=sizeof value)return fail(error,"missing fixture field "+std::to_string(address));
    std::memcpy(&value,found->second.data(),sizeof value);return true;
}
bool ap(uintptr_t object,uintptr_t image,uintptr_t expected,std::string& error){uintptr_t actual=0;return at(object,0,actual,error)&&(actual==image+expected||fail(error,"live native class addresspoint differs"));}
namespace native {
#include "native_types.inc"
struct WorldStatus {int32_t saves_in_progress=0,edit_operations=0;};
constexpr uintptr_t clock_slot=0x13df9a0,mode_slot=0x13df320,game_slot=0x13df8f0;
std::atomic<uint32_t> game_thread=17;
bool ready(){return fixture->active;}
bool check_thread(std::string& error){error.clear();return fixture->clock_thread||fail(error,"engine operation rejected: caller is not the bound game thread");}
template<class T> bool read_at(const void* base,size_t offset,T& value){std::string error;return at(uintptr_t(base),offset,value,error);}
void* singleton(uintptr_t offset){uintptr_t value=0;std::string error;return at(image_base,offset,value,error)?reinterpret_cast<void*>(value):nullptr;}
bool has_vtable(const void* object,uintptr_t rva){std::string error;return ap(uintptr_t(object),image_base,rva+0x10,error);}
#include "native_bodies.inc"
}
bool current_scope(const Context& context,const Binding& binding,Pin& p,native::WorldStatus& state,std::string& error){
    p={};if(!fixture->scope_valid)return fail(error,"original current origin failure");
    p.context=context;p.binding=binding;p.common_epoch=19;p.image=image_base;p.thread=17;p.world=0x72000000;
    state.saves_in_progress=fixture->saves;state.edit_operations=fixture->edits;return true;
}
#include "owner_ledger.inc"
#include "owner_activity.inc"
struct NativePort {
    Context context_;Binding binding_;
    bool baseline(const Pin&,std::string&,int32_t=0);
    bool current_owners(const Pin&,std::string&,int32_t=0);
};
#include "owner_bodies.inc"
struct Result {bool ok=false;std::string error;std::vector<std::pair<uintptr_t,size_t>> reads;};
Pin expected_pin(){Fixture fresh;fixture=&fresh;Pin value;float pause=0;std::string error;need(once(Context{},Binding{},value,pause,error),"baseline fixture "+error);fixture=nullptr;return value;}
Result run(bool candidate,const Pin& expected,const std::function<void(Fixture&)>& mutate={},int32_t expected_edits=0){
    Fixture current;fixture=&current;if(mutate)mutate(current);NativePort port;port.context_=expected.context;port.binding_=expected.binding;
    Result out;out.ok=candidate?port.current_owners(expected,out.error,expected_edits):port.baseline(expected,out.error,expected_edits);
    out.reads=current.reads;fixture=nullptr;need(apply_timing::readers==0,"owner reader scope escaped");return out;
}
int main(){try{
    const auto expected=expected_pin();const auto before=run(false,expected),after=run(true,expected);
    need(before.ok&&after.ok,"nonempty History and20 nonzero categories accepted");
    need(after.reads.size()<before.reads.size(),"owner path removes full scalar capture");
    const std::vector<std::function<void(Fixture&)>> failures={
        [](Fixture& f){f.scope_valid=false;},[](Fixture& f){f.saves=1;},[](Fixture& f){f.edits=1;},
        [](Fixture& f){f.put(clock_base,uintptr_t(0));},[](Fixture& f){f.put(business_base,uintptr_t(0));},[](Fixture& f){f.put(record_base,uintptr_t(0));},
        [](Fixture& f){f.put(record_base+0x18,int32_t(1));},[](Fixture& f){f.put(business_base+0x70,int32_t(0));},
        [](Fixture& f){f.put(business_base+0x74,int32_t(2));},[](Fixture& f){f.put(history_base+8,uintptr_t(0));},
        [](Fixture& f){f.put(history_base,record_base);},[](Fixture& f){f.put(clock_base+0x58,1.f);},[](Fixture& f){f.put(clock_base+0x5c,0.f);},
        [](Fixture& f){f.active=false;},[](Fixture& f){f.clock_thread=false;},
        [](Fixture& f){f.put(subscribers_base,uintptr_t(0));},[](Fixture& f){f.put(subscribers_base+0x90,int32_t(-1));},
        [](Fixture& f){f.put(mode_base+0x304,uint8_t(1));},[](Fixture& f){f.put(clock_base+0x58,NAN);}
    };
    for(const auto& mutate:failures){const auto old=run(false,expected,mutate),now=run(true,expected,mutate);need(!old.ok&&!now.ok,"owner/domain failure accepted");need(old.error==now.error,"original cause changed: "+old.error+" / "+now.error);}
    for(size_t i=0;i<20;++i){
        const auto line=record_base+0x30+i*0x48;
        for(unsigned kind=0;kind<3;++kind){auto mutate=[=](Fixture& f){if(kind==0)f.put(line,uintptr_t(0));if(kind==1)f.put(line+0x20,int32_t(100));if(kind==2)f.put(line+0x18,int32_t(-1));};
            const auto old=run(false,expected,mutate),now=run(true,expected,mutate);need(!old.ok&&!now.ok&&old.error==now.error,"category identity/index/nonnegative count failure changed");}
    }
    for(const auto& [address,bytes]:after.reads){(void)bytes;const auto old=run(false,expected,[=](Fixture& f){f.fail_address=address;}),now=run(true,expected,[=](Fixture& f){f.fail_address=address;});need(!old.ok&&!now.ok,"retained owner read failure accepted");}
    for(unsigned changed=0;changed<12;++changed){auto required=expected;switch(changed){case 0:++required.common_epoch;break;case 1:++required.image;break;case 2:++required.world;break;case 3:++required.thread;break;case 4:++required.clock;break;case 5:++required.business;break;case 6:++required.record;break;case 7:++required.history;break;case 8:++required.history_count;break;case 9:++required.history_capacity;break;case 10:required.selected=3;break;case 11:required.pause=0;break;}need(!run(true,required).ok,"changed compared owner accepted");}
    auto client=expected;client.context.role=action_session::Role::Client;need(run(true,client).ok,"Client zero Clock owner rejected");
    client.selected=3;need(!run(true,client,[](Fixture& f){f.put(clock_base+0x58,3.f);}).ok,"Client nonzero Clock accepted");
    auto scalars=[](Fixture& f){f.put(business_base+0x38,int32_t(901));f.put(record_base+0x20,int32_t(902));for(size_t i=0;i<20;++i)f.put(record_base+0x30+i*0x48+0x1c,int32_t(i+903));};
    need(run(false,expected,scalars).ok&&run(true,expected,scalars).ok,"discarded values wrongly treated as owners");
    need(run(true,expected,[](Fixture& f){f.edits=1;},1).ok,"explicit expected edit count rejected");
    Fixture throwing;fixture=&throwing;throwing.throw_address=record_base+0x30;NativePort port;std::string error;bool caught=false;
    try{port.current_owners(expected,error);}catch(const std::runtime_error& e){caught=std::string(e.what())=="original byte reader exception";}
    need(caught&&apply_timing::readers==0,"exception was suppressed or leaked owner scope");fixture=nullptr;
    std::cout<<"PASS owner-only actual bodies: "<<assertions<<" assertions; per-check reads "<<before.reads.size()<<" -> "<<after.reads.size()<<"; full once/read unchanged\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"error: owner-only fixture: "<<e.what()<<'\n';return 1;}}
