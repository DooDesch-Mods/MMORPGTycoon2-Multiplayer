#include "subscriber_lifecycle/runtime_append.h"
#include "live_host_state/apply_timing.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace fixture {
unsigned checks=0,enters=0,leaves=0,operations=0,guards=0,closed_reads=0,held_reads=0;
unsigned fail_operation=0,fail_guard=0,root_reads=0;
bool bad_census=false;
bool held=false,fail_enter=false,fail_leave=false,fail_closed=false,drift=false,throw_body=false,pending_tasks=false;
std::vector<std::string> mutations;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
void reset(){root_reads=0;bad_census=false;enters=leaves=operations=guards=closed_reads=held_reads=fail_operation=fail_guard=0;
    held=fail_enter=fail_leave=fail_closed=drift=throw_body=pending_tasks=false;mutations.clear();}
}
namespace mt2mp::subscriber_lifecycle {
uint32_t call(SaveScopeCall fn,void*){
    if(reinterpret_cast<uintptr_t>(fn)==0x10000000+0x4a03a0){
        ++fixture::enters;if(fixture::fail_enter)return 77;
        fixture::require(!fixture::held,"nested native enter");fixture::held=true;
    }else{++fixture::leaves;if(fixture::fail_leave)return 88;
        fixture::require(fixture::held,"leave without held scope");fixture::held=false;}
    return 0;
}
#include "save_scope_actual.inc"
}
namespace subject {
namespace local=mt2mp::subscriber_lifecycle;
using FrameBinding=mt2mp::game::subscriber_lifecycle_append::FrameBinding;
using QuestLinkRequest=mt2mp::game::subscriber_lifecycle_append::QuestLinkRequest;
using QuestLinkReceipt=mt2mp::game::subscriber_lifecycle_append::QuestLinkReceipt;
using ProgressRequest=mt2mp::game::subscriber_lifecycle_append::ProgressRequest;
using ProgressReceipt=mt2mp::game::subscriber_lifecycle_append::ProgressReceipt;
namespace stage {
struct ClientWorld {
    struct Origin {void* mode=reinterpret_cast<void*>(1);uint64_t world_epoch=1,generation=2;} origin;
    uintptr_t image=0x10000000;unsigned thread=3;
};
bool same_client_owner(const ClientWorld& a,const ClientWorld& b){return a.image==b.image&&a.thread==b.thread&&a.origin.mode==b.origin.mode&&a.origin.world_epoch==b.origin.world_epoch&&a.origin.generation==b.origin.generation;}
}
struct Work {stage::ClientWorld original;std::string origin,local_sha;const FrameBinding* frame=nullptr;};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
bool hook_ready=true;
uintptr_t image_base=0x10000000;
struct Api {uintptr_t quest_resolve=1,progress_construct=1,progress_destroy=1,progress_npc_assign=1,progress_add=1,progress_shift=1;} api;
uint64_t blocked_epoch=0,blocked_generation=0;std::string blocked_cause;
bool fail(std::string& e,const std::string& message){e=message;return false;}
bool checked_roots(bool expected,stage::ClientWorld& out,std::string& e){
    if(expected!=fixture::held)return fail(e,"original exact SaveScope/scheduler state differs");
    if(expected&&fixture::pending_tasks)return fail(e,"Subscriber append original TaskManager not drained");
    if(!expected&&fixture::leaves&&fixture::fail_closed)return fail(e,"original closed owner read failure");
    out={};if(fixture::drift)++out.origin.world_epoch;return true;
}
bool read_world(Work&,bool expected,stage::ClientWorld& out,std::string& e){
    if(expected)++fixture::held_reads;else ++fixture::closed_reads;
    return checked_roots(expected,out,e)&&(!fixture::bad_census||fail(e,"full census corruption"));
}
bool read_world_roots(Work&,bool expected,stage::ClientWorld& out,std::string& e){
    ++fixture::root_reads;return checked_roots(expected,out,e);
}
#include "quest_edit_batch.inc"
struct QuestLinkWork {Work world;QuestLinkRequest request;QuestLinkReceipt* receipt=nullptr;
    bool armed=false,unknown=false;std::unique_ptr<QuestLinkWork> retained_next;};
struct ProgressWork {Work world;ProgressRequest request;ProgressReceipt* receipt=nullptr;
    bool retire=false,unknown=false;uintptr_t selected=0;std::unique_ptr<ProgressWork> retained_next;};
std::unique_ptr<QuestLinkWork> retained_quest_links;
std::unique_ptr<ProgressWork> retained_progress;
struct Lookup {local::LookupReceipt receipt(){return {};};void clear(){}} lookup_scope;
bool fresh(std::string& e){
    ++fixture::guards;
    if(fixture::guards==fixture::fail_guard)return fail(e,"original helper owner guard failure");
    return fixture::held||fail(e,"original body requires held scope");
}
bool operation(const char* name,std::string& e){
    if(!fresh(e))return false;
    ++fixture::operations;fixture::mutations.push_back(name);
    if(fixture::throw_body)throw std::runtime_error("original body exception");
    return fixture::operations!=fixture::fail_operation||fail(e,"original body mutation failure");
}
bool quest_link_body(void* value,std::string& e){auto& w=*static_cast<QuestLinkWork*>(value);
    w.receipt->resolve.attempted=true;
    const bool ok=operation("bind",e);w.receipt->after_confirmed=ok;if(!ok)w.unknown=true;return ok;}
bool quest_link_exit(void*,std::string& e){return fresh(e);}
bool progress_body(void* value,std::string& e){auto& w=*static_cast<ProgressWork*>(value);
    if(w.retire)w.receipt->destroy.attempted=true;else w.receipt->allocate.attempted=true;
    const bool ok=operation(w.retire?"retire":"ensure",e);
    w.receipt->progress=4096;w.receipt->after_confirmed=ok;if(!ok)w.unknown=true;return ok;}
bool progress_exit(void*,std::string& e){return fresh(e);}
#include "quest_wrappers_actual.inc"
bool unused_fresh(void*,const local::ExternalLink&,std::string&){return true;}
FrameBinding frame;
ProgressRequest progress{1000,2000,"npc",unused_fresh,nullptr};
QuestLinkRequest link{3000,4000,"npc/quest/1",unused_fresh,nullptr};
void reset(){fixture::reset();busy.clear();blocked_epoch=blocked_generation=0;blocked_cause.clear();
    retained_quest_batches.reset();retained_progress.reset();retained_quest_links.reset();frame={};frame.local_sha="sha";}
bool sequence(QuestEditBatch& batch,std::string& e){
    ProgressReceipt a,b;QuestLinkReceipt c;
    if(!retire_progress(batch,progress,5000,"world",frame,a,e))return false;
    fixture::require(!a.confirmed,"held retirement receipt cannot claim closed confirmation");
    if(!ensure_progress(batch,progress,"world",frame,b,e))return false;
    fixture::require(!b.confirmed,"held ensure receipt cannot claim closed confirmation");
    if(!bind_quest_instance(batch,link,"world",frame,c,e))return false;
    fixture::require(!c.confirmed,"held bind receipt cannot claim closed confirmation");return true;
}
bool final_guard(std::string& e){return fresh(e);}
}
int main(){try{
    using namespace subject;using fixture::require;
    std::string e;local::SaveScopeReceipt receipt;
    reset();ProgressReceipt a,b;QuestLinkReceipt c;
    require(subject::retire_progress(progress,5000,"world",frame,a,e)&&a.confirmed,"standalone retirement");
    require(subject::ensure_progress(progress,"world",frame,b,e)&&b.confirmed,"standalone ensure");
    require(subject::bind_quest_instance(link,"world",frame,c,e)&&c.confirmed,"standalone bind");
    const auto original=fixture::mutations;
    require(fixture::enters==3&&fixture::leaves==3&&!fixture::held,"standalone three exact scopes");
    reset();require(with_quest_edits("world",frame,sequence,final_guard,receipt,e),"batched sequence");
    require(fixture::enters==1&&fixture::leaves==1&&!fixture::held&&fixture::mutations==original,"one scope preserves operation order");
    require(receipt.enter_returned&&receipt.leave_returned&&!receipt.unknown,"batch confirms closed scope");
    require(fixture::held_reads==3&&fixture::closed_reads==2,"each subsequent helper and final boundary require held owners; closed admission/readback retained");
    reset();require(with_quest_edits("world",frame,[](QuestEditBatch&,std::string&){return true;},final_guard,receipt,e),"no-op body");
    require(!fixture::enters&&!fixture::leaves&&!fixture::closed_reads&&!receipt.enter_attempted,"no-op has no native enter or extra world read");
    reset();fixture::fail_operation=2;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e),"second operation failure");
    require(fixture::operations==2&&fixture::enters==1&&fixture::leaves==1&&receipt.unknown&&e.find("original body mutation failure")!=std::string::npos,"stop on failure with cleanup and original cause");
    const auto calls=fixture::enters;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::enters==calls,"unknown epoch does not replay");
    reset();fixture::fail_enter=true;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::enters==1&&!fixture::operations&&!fixture::leaves&&receipt.unknown,"failed native enter has no body or leave");
    reset();fixture::fail_leave=true;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::operations==3&&fixture::leaves==1&&receipt.unknown&&e.find("Decrement exception=88")!=std::string::npos,"failed leave remains Unknown");
    reset();fixture::fail_guard=2;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::operations==1&&!fixture::leaves&&receipt.unknown,"failed helper exit guard prohibits leave and later helpers");
    reset();fixture::throw_body=true;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::operations==1&&fixture::leaves==1&&receipt.unknown&&e.find("original body exception")!=std::string::npos,"body exception preserves cause and does not replay");
    reset();fixture::fail_closed=true;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::leaves==1&&receipt.unknown&&e=="original closed owner read failure","closed read remains mandatory");
    reset();
    const auto changed_owner=[](QuestEditBatch& batch,std::string& error){ProgressReceipt first,second;
        if(!ensure_progress(batch,progress,"world",frame,first,error))return false;
        fixture::drift=true;return ensure_progress(batch,progress,"world",frame,second,error);};
    require(!with_quest_edits("world",frame,changed_owner,final_guard,receipt,e)&&fixture::operations==1&&!fixture::leaves&&receipt.unknown,"held owner drift stops next mutation and unsafe leave");
    reset();
    const auto ignored_failure=[](QuestEditBatch& batch,std::string& error){ProgressReceipt first;fixture::fail_operation=1;
        const bool attempted=ensure_progress(batch,progress,"world",frame,first,error);
        fixture::require(!attempted,"injected failure occurred");return true;};
    require(!with_quest_edits("world",frame,ignored_failure,final_guard,receipt,e)&&receipt.unknown,"caller cannot turn ignored helper failure into confirmation");
    reset();
    const auto two_rows=[](QuestEditBatch& batch,std::string& error){
        if(!with_quest_edit_row(batch,sequence,final_guard,error))return false;
        fixture::require(quest_edits_held(batch)&&verify_quest_edits_held(batch,error),"first row leaves owned scope held");
        return with_quest_edit_row(batch,sequence,final_guard,error);
    };
    require(with_quest_edits("world",frame,two_rows,final_guard,receipt,e),"two Toon rows share explicit phase close");
    require(fixture::enters==1&&fixture::leaves==1&&fixture::operations==6&&!fixture::held,"two complete rows have one original enter and leave");
    reset();
    const auto row_exit_failure=[](QuestEditBatch& batch,std::string& error){
        const auto reject=[](std::string& cause){return fail(cause,"row exit fence failure");};
        return with_quest_edit_row(batch,sequence,reject,error);
    };
    require(!with_quest_edits("world",frame,row_exit_failure,final_guard,receipt,e),"row exit failure rejects phase");
    require(fixture::operations==3&&!fixture::leaves&&receipt.unknown&&e.find("row exit fence failure")!=std::string::npos,"failed row guard forbids unsafe leave and preserves cause");
    reset();fixture::fail_operation=5;
    require(!with_quest_edits("world",frame,two_rows,final_guard,receipt,e)&&fixture::operations==5&&fixture::leaves==1&&receipt.unknown,"later row body failure keeps guarded explicit cleanup and Unknown");
    reset();
    const auto drift_between=[](QuestEditBatch& batch,std::string& error){
        if(!with_quest_edit_row(batch,sequence,final_guard,error))return false;
        fixture::drift=true;
        if(!verify_quest_edits_held(batch,error))return false;
        return with_quest_edit_row(batch,sequence,final_guard,error);
    };
    require(!with_quest_edits("world",frame,drift_between,final_guard,receipt,e)&&fixture::operations==3&&!fixture::leaves&&receipt.unknown,"between-row owner drift rejects before next mutation");
    reset();
    const auto no_rows=[](QuestEditBatch& batch,std::string& error){
        const auto noop=[](QuestEditBatch&,std::string&){return true;};
        return with_quest_edit_row(batch,noop,final_guard,error)&&with_quest_edit_row(batch,noop,final_guard,error);
    };
    require(with_quest_edits("world",frame,no_rows,final_guard,receipt,e)&&!fixture::enters&&!fixture::leaves&&!fixture::closed_reads,"two no-op rows do not open original scope");
    reset();
    const auto task_after_row=[](QuestEditBatch& batch,std::string& error){
        if(!with_quest_edit_row(batch,sequence,final_guard,error))return false;
        fixture::pending_tasks=true;
        return with_quest_edit_row(batch,sequence,final_guard,error);
    };
    require(!with_quest_edits("world",frame,task_after_row,final_guard,receipt,e),"pending TaskManager work rejects next held row");
    require(fixture::operations==3&&fixture::enters==1&&!fixture::leaves&&receipt.unknown&&e.find("Subscriber append original TaskManager not drained")!=std::string::npos,"task failure retains exact cause and prevents next mutation or unsafe leave");
    const auto task_enters=fixture::enters;
    require(!with_quest_edits("world",frame,sequence,final_guard,receipt,e)&&fixture::enters==task_enters,"task failure Unknown does not replay original entry");
    reset();
    const auto corrupt_before_helper=[](QuestEditBatch& batch,std::string& error){
        if(!sequence(batch,error))return false;
        fixture::bad_census=true;
        fixture::require(verify_quest_edits_held(batch,error),"held roots do not substitute for full mutation admission");
        ProgressReceipt next;return ensure_progress(batch,progress,"world",frame,next,error);
    };
    require(!with_quest_edits("world",frame,corrupt_before_helper,final_guard,receipt,e)&&fixture::operations==3&&!fixture::leaves&&receipt.unknown&&e.find("full census corruption")!=std::string::npos,"next native helper keeps full census admission and blocks mutation");
    reset();
    const auto corrupt_at_row=[](QuestEditBatch& batch,std::string& error){
        const auto corrupt=[](QuestEditBatch& row,std::string& cause){if(!sequence(row,cause))return false;fixture::bad_census=true;return true;};
        return with_quest_edit_row(batch,corrupt,final_guard,error);
    };
    require(!with_quest_edits("world",frame,corrupt_at_row,final_guard,receipt,e)&&fixture::operations==3&&!fixture::leaves&&receipt.unknown&&e.find("full census corruption")!=std::string::npos,"row exit keeps full census and prohibits unsafe leave");
    reset();
    const auto corrupt_at_close=[](QuestEditBatch& batch,std::string& error){
        if(!sequence(batch,error))return false;
        fixture::bad_census=true;
        return verify_quest_edits_held(batch,error);
    };
    require(!with_quest_edits("world",frame,corrupt_at_close,final_guard,receipt,e)&&fixture::operations==3&&fixture::root_reads==1&&!fixture::leaves&&receipt.unknown&&e.find("full census corruption")!=std::string::npos,"phase close retains full census after root-only trailing check");
    std::cout<<fixture::checks<<" Quest scope batch assertions passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
