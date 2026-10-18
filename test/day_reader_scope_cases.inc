#include <algorithm>
#include <array>
#include <map>
#include <vector>
namespace day_scope_test {
namespace apply_timing=timing;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
struct Context {int id=1;bool operator==(const Context&)const=default;};
struct Binding {int native_epoch=2,native_generation=3;};
struct Record {int values=0,lines=0;bool operator==(const Record&)const=default;};
struct Values {std::array<int,6> calendar{};int cash=0,record=0,lines=0;bool operator==(const Values&)const=default;};
struct Pin {
    Context context;Binding binding;int common_epoch=0;uintptr_t world=0,image=0,clock=0,business=0,history=0,record=0;
    uint32_t thread=0;int32_t history_count=0,history_capacity=0;float selected=0,pause=0;Values values;
    bool operator==(const Pin& p)const {return context==p.context&&binding.native_epoch==p.binding.native_epoch&&binding.native_generation==p.binding.native_generation&&common_epoch==p.common_epoch&&world==p.world&&image==p.image&&clock==p.clock&&business==p.business&&history==p.history&&record==p.record&&thread==p.thread&&history_count==p.history_count&&history_capacity==p.history_capacity&&selected==p.selected&&pause==p.pause&&values==p.values;}
};
struct NativeHistory {Pin pin;std::vector<uintptr_t> owners;std::vector<Record> records;};
constexpr size_t history_limit=10;
constexpr uintptr_t clock=0x10000,business=0x20000,history=0x30000,record=0x40000;
std::map<std::pair<uintptr_t,size_t>,int64_t> fields;
int pass=0,field_reads=0,record_reads=0,fail_pass=0,throw_pass=0;
bool drift_record=false,drift_owner=false,drift_header=false;
int saved=0,edits=0,held_checks=0;
namespace subscriber_lifecycle_append {
struct QuestEditBatch {bool valid=true;};
bool verify_quest_edits_held(QuestEditBatch& b,std::string& e){++held_checks;return b.valid||fail(e,"original owned held proof failure");}
}
void reset(){
    saved=edits=held_checks=0;pass=field_reads=record_reads=fail_pass=throw_pass=0;drift_record=drift_owner=drift_header=false;fields.clear();
    const auto image=uintptr_t(GetModuleHandleW(nullptr));
    fields[{image,0x13df9a0}]=clock;fields[{image,0x13df220}]=business;
    fields[{business,0x68}]=history;fields[{business,0x70}]=1;fields[{business,0x74}]=2;fields[{business,0x100}]=record;
    fields[{history,0}]=record;fields[{clock,0x5c}]=1;
}
template<class T>bool at(uintptr_t base,size_t offset,T& out,std::string& e){
    ++field_reads;unsigned char live=0;if(!memory.copy(uintptr_t(allocation),&live,1,e))return false;
    auto value=fields[{base,offset}];
    if(drift_header&&base==business&&offset==0x70&&record_reads>0)value=2;
    out=static_cast<T>(value);return true;
}
bool ap(uintptr_t,uintptr_t,uintptr_t,std::string& e){unsigned char live=0;return memory.copy(uintptr_t(allocation),&live,1,e);}
namespace native {
struct WorldStatus {int saves_in_progress=0,edit_operations=0;};
bool read_world_status(WorldStatus& out,std::string&){out={saved,edits};return true;}
bool read_resolver_receipt(uintptr_t& image,uint32_t& thread,std::string& sha,std::string& e){
    ++pass;if(pass==throw_pass)throw std::runtime_error("day original reader exception");
    if(pass==fail_pass)return fail(e,"day original receipt failure");
    image=uintptr_t(GetModuleHandleW(nullptr));thread=GetCurrentThreadId();sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";return true;
}
}
namespace action_session {
struct Session {bool current(Context& out,std::string&){out={};return true;}};
Session& selected_session(){static Session s;return s;}
}
namespace building_mixed_identity::opened_file {struct CurrentWorld {int world_epoch=2,generation=3;void* world=reinterpret_cast<void*>(uintptr_t(0x50000));void* state=reinterpret_cast<void*>(uintptr_t(1));};}
namespace native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out={};if(drift_owner&&pass==2)out.world=reinterpret_cast<void*>(uintptr_t(0x50010));return true;}
}
namespace net {int current_world_epoch(){return 7;}}
struct NativePort {
    Context context_;Binding binding_;
    bool read_once(NativeHistory&,std::string&,bool share_metadata=false,subscriber_lifecycle_append::QuestEditBatch* held=nullptr);
    bool read_quest_held(NativeHistory&,subscriber_lifecycle_append::QuestEditBatch&,std::string&);
    bool read(NativeHistory&,std::string&);
    bool record_at(uintptr_t,uintptr_t,Record& out,std::string& e){
        ++record_reads;unsigned char live=0;if(!memory.copy(uintptr_t(allocation),&live,1,e))return false;
        out.values=live+(drift_record&&pass==2?1:0);out.lines=8;return true;
    }
};
#include "day_reader_entries.inc"
}
void day_reader_scope_cases(){
    namespace d=day_scope_test;
    d::NativePort port;d::NativeHistory result;std::string error;
    d::reset();auto start=queries();SetLastError(673);
    need(port.read(result,error),"actual Day double reader succeeds without parent");
    need(d::pass==2&&d::record_reads==4,"both complete Day passes and current-record rereads execute");
    const auto full_reads=d::field_reads;
    need(queries()-start==1,"two Day passes share one actual OS metadata query");
    need(GetLastError()==673&&timing::page_read_scope==nullptr,"Day fallback ends and preserves LastError");
    d::reset();
    {timing::PageReadScope outer;unsigned char value=0;need(memory.copy(uintptr_t(allocation),&value,1,error),"prime outer metadata");start=queries();
        need(port.read(result,error),"Day joins current pure caller scope");
        need(memory.copy(uintptr_t(allocation),&value,1,error)&&queries()==start,"Day does not invalidate pure caller metadata");
        need(d::field_reads==full_reads&&d::pass==2&&d::record_reads==4,"joining preserves every actual live read");
        allocation[0]=91;d::reset();need(port.read(result,error)&&result.pin.values.record==91,"shared metadata never caches live record values");
    }
    d::reset();start=queries();need(port.read_once(result,error,true),"private opt-in has no-parent fallback");need(queries()-start==1&&!timing::page_read_scope,"private fallback scope is bounded");
    d::reset();
    {timing::PageReadScope outer;unsigned char value=0;need(memory.copy(uintptr_t(allocation),&value,1,error),"default outer prime");start=queries();
        need(port.read_once(result,error),"private default read remains available");need(memory.copy(uintptr_t(allocation),&value,1,error)&&queries()-start==2,"default private nested reader retains fresh entry and exit semantics");}
    for(int which=1;which<=2;++which){d::reset();d::fail_pass=which;error.clear();need(!port.read(result,error)&&error=="day original receipt failure"&&d::pass==which,"first or second pass error retains original cause");need(!timing::page_read_scope,"failed Day read ends metadata scope");}
    d::reset();d::drift_record=true;need(!port.read(result,error)&&error=="Day actual History/45 fields changed during double read","second pass value drift is rejected");
    d::reset();d::drift_owner=true;need(!port.read(result,error)&&error=="Day actual History/45 fields changed during double read","second pass owner drift is rejected");
    d::reset();error.clear();d::drift_header=true;need(!port.read(result,error)&&error=="Day actual History changed during read","within-pass History header drift is rejected");
    d::reset();d::throw_pass=2;bool caught=false;try{port.read(result,error);}catch(const std::runtime_error& x){caught=std::string(x.what())=="day original reader exception";}need(caught&&!timing::page_read_scope,"second pass exception propagates and unwinds");
    d::reset();
    {timing::PageReadScope outer;need(port.read(result,error),"prime before original boundary");DWORD previous=0;
        {timing::PageReadScope original(timing::PageBoundary::OriginalCall);need(VirtualProtect(allocation,4096,PAGE_NOACCESS,&previous)!=0,"original boundary changes private test page");}
        need(!port.read(result,error)&&error=="error: native placement UI page unreadable","original boundary forces current page permissions");DWORD ignored=0;need(VirtualProtect(allocation,4096,previous,&ignored)!=0,"restore private test page");}
    d::subscriber_lifecycle_append::QuestEditBatch held;
    d::reset();d::saved=1;d::fields[{d::clock,0x5c}]=0;
    need(port.read_quest_held(result,held,error),"dedicated owned held History reader");
    need(result.pin.pause==0&&result.pin.selected==0&&d::held_checks==2&&d::pass==2&&d::field_reads==full_reads&&d::record_reads==4,"held read preserves actual Clock and both complete fresh passes");
    d::reset();d::saved=1;need(!port.read(result,error)&&error.find("requires0")!=std::string::npos,"public closed reader still rejects any open scope");
    for(int invalid=0;invalid<5;++invalid){
        d::reset();d::saved=1;d::fields[{d::clock,0x5c}]=0;held.valid=true;
        if(invalid==0)d::saved=0;if(invalid==1)d::saved=2;if(invalid==2)d::edits=1;
        if(invalid==3)d::fields[{d::clock,0x5c}]=1;if(invalid==4)held.valid=false;
        need(!port.read_quest_held(result,held,error),"held route rejects wrong owner/save/edit/raw Clock state");
    }
    held.valid=true;
    for(int drift=0;drift<3;++drift){
        d::reset();d::saved=1;d::fields[{d::clock,0x5c}]=0;
        d::drift_record=drift==0;d::drift_owner=drift==1;d::drift_header=drift==2;
        need(!port.read_quest_held(result,held,error),"held History retains record/owner/header drift rejection");
    }
    need(!timing::page_read_scope,"Day tests retain no cross-call metadata");
}
