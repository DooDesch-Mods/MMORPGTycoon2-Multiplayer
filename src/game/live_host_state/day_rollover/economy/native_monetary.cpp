#include "native_monetary.h"
#include "toon_weapon_visual/visual.h"
#include "state_difference.h"
#include "live_host_state/apply_timing.h"
#include "live_host_state/native_bulk_read.h"
#include "subscriber_store_owner.h"
#include "subscriber_order.h"
#include "sample_region_index.h"
#include "capture_readback.h"
#include "capture_pause_restore.h"
#include <Windows.h>
#include "live_host_state/scalar_store.h"
#include "../pointer_store.h"
#include "placement_ui_probe/ui_probe.h"
#include <algorithm>
#include <set>
#include <map>
#include <bit>
#include "subscriber_lifecycle/runtime_capture.h"
#include "subscriber_lifecycle/runtime_append.h"
#include "subscriber_lifecycle/append_calls.h"
#include "subscriber_lifecycle/retirement_runtime.h"
#include "native_open_runtime.h"
#include "region_saved_identity.h"
#include "session_provider.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/game/native_engine.h"
#include "npc_probe.h"
#include "npc_quest_visitor_reader.h"
#include "npc_quest_daily_apply.h"
#include "subscriber_lifecycle/save_scope.h"
#include <functional>
#include <cstring>
namespace mt2mp::game::live_host_state::day::economy {
namespace {
placement_ui_probe::ProcessMemory memory;
uint32_t call_capture_pause(uintptr_t setter,uintptr_t clock,float pause){
    __try{reinterpret_cast<void(*)(uintptr_t,float)>(setter)(clock,pause);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
constexpr size_t sub_fields[]={0x2c,0x30,0x34,0x48,0x4c,0x9c};
bool fail(std::string& e,const std::string& s){e=s;return false;}
template<class T>bool at(uintptr_t p,size_t n,T& v,std::string& e){if(!p||p>UINTPTR_MAX-n)return fail(e,"economic actual native address extent invalid");return memory.copy(p+n,&v,sizeof v,e);}
bool ap(uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){uintptr_t value=0;return at(p,0,value,e)&&(value==image+expected||fail(e,"economic actual class addresspoint differs"));}
bool text(uintptr_t p,size_t offset,size_t bound,std::string& out,std::string& e){uintptr_t bytes=0;uint64_t count=0;if(!at(p,offset,bytes,e)||!at(p,offset+8,count,e)||count>bound||!bytes||bytes>UINTPTR_MAX-count)return fail(e,e.empty()?"economic actual GNU string bounds invalid":e);out.resize(size_t(count));if(count&&!memory.copy(bytes,out.data(),out.size(),e))return false;char end=1;if(!at(bytes,size_t(count),end,e)||end||out.find('\0')!=std::string::npos)return fail(e,"economic actual GNU string terminator differs");return true;}
template<class Row>bool loans(const Pin& pin,size_t offset,bool offer,uintptr_t& data,std::vector<uintptr_t>& owners,std::vector<Row>& out,std::string& e){int32_t count=-1,capacity=-1;if(!at(pin.business,offset+8,data,e)||!at(pin.business,offset+0x10,count,e)||!at(pin.business,offset+0x14,capacity,e)||count<0||capacity<count||count>1024||(!data&&count))return fail(e,e.empty()?"economic native Loan/Offer bounded array differs":e);std::set<std::string> ids;for(int32_t i=0;i<count;++i){uintptr_t p=0;Row row;if(!at(data,size_t(i)*8,p,e)||!ap(p,pin.image,offer?0x16df370:0x17438e0,e))return false;for(size_t n=0;n<3;++n)if(!text(p,0x18+n*0x20,n?2048:256,row.text[n],e))return false;if(!ids.insert(row.text[0]).second)return fail(e,"economic native Loan/Offer duplicate ID");for(size_t n=0;n<row.amounts.size();++n)if(!at(p,0x78+n*4,row.amounts[n],e))return false;owners.push_back(p);out.push_back(std::move(row));}return true;}
}
#include "native_motion.inc"
#include "native_weapon_state.inc"
#include "native_quest_state.inc"
bool NativePort::bind_actual_receipt(const Target& target,std::string& error){
    if(!day::valid(target,error))return false;
    NativeHistory actual;if(!history_.read(actual,error))return false;
    const auto& pin=actual.pin;
    if(pin.context.session!=target.frame.session||pin.context.snapshot_uuid!=target.frame.snapshot||pin.common_epoch!=target.frame.epoch||pin.values!=target.frame.values||actual.records.size()!=target.history.size())return fail(error,"economic actual Day receipt bind differs from current native world");
    for(size_t i=0;i<actual.records.size();++i)if(actual.records[i]!=target.history[i].record)return fail(error,"economic actual Day allocation content differs before bind");
    actual_day_receipt_=target;error.clear();return true;
}
bool NativePort::once(Owners& out,std::string& e,std::vector<QuestOwner>* npc_owners){apply_timing::PageReadScope pages;out={};MotionSamplePass motion_pass;if(!history_.read(out.history,e))return false;const auto& p=out.history.pin;if(p.context.session!=actual_day_receipt_.frame.session||p.context.snapshot_uuid!=actual_day_receipt_.frame.snapshot||p.common_epoch!=actual_day_receipt_.frame.epoch||p.values!=actual_day_receipt_.frame.values||out.history.records.size()!=actual_day_receipt_.history.size())return fail(e,"economic actual52Scalar/day receipt differs before owner read");for(size_t i=0;i<out.history.records.size();++i)if(out.history.records[i]!=actual_day_receipt_.history[i].record)return fail(e,"economic actual History differs from retained Day allocation receipt");out.state.day=actual_day_receipt_;std::vector<QuestOwner> quest_owners;if(!npc_probe::action_runtime::state_owners(p.context,p.binding,quest_owners,e)||!quest_npcs(p,quest_owners,out.state.npcs,e))return false;int32_t count=-1,capacity=-1;if(!at(p.image,0x13df690,out.subscriber_manager,e)||!ap(out.subscriber_manager,p.image,0x1725f40,e)||!at(out.subscriber_manager,0x40,out.subscriber_data,e)||!at(out.subscriber_manager,0x48,count,e)||!at(out.subscriber_manager,0x4c,capacity,e)||count<0||capacity<count||count>1000000||(!out.subscriber_data&&count))return fail(e,e.empty()?"economic actual Subscriber manager bounds differ":e);if(!at(out.subscriber_manager,0x68,out.next_id,e)||out.next_id<0)return fail(e,"economic native nextID invalid");std::set<int32_t> ids;for(int32_t i=0;i<count;++i){uintptr_t owner=0;Subscriber row;if(!at(out.subscriber_data,size_t(i)*8,owner,e)||!ap(owner,p.image,0x16e75f0,e)||!at(owner,0x20,row.serialized_id,e)||row.serialized_id<0||row.serialized_id>=out.next_id||!ids.insert(row.serialized_id).second)return fail(e,e.empty()?"economic actual Subscriber ID ownership differs":e);for(size_t n=0;n<5;++n)if(!at(owner,sub_fields[n],row.pay[n],e))return false;if(!at(owner,0x9c,row.play_days,e))return false;subscriber_needs::Owner needs_owner;if(!subscriber_needs::sample([](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},p.image,owner,row.serialized_id,needs_owner,row.needs_bits,e))return false;out.needs_owners.push_back(needs_owner);if(!subscriber_activity::sample([](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},p.image,owner,row.serialized_id,row.activity,e))return false;if(!motion_read(p,owner,row.motion,e,nullptr,nullptr,&motion_pass))return false;constexpr size_t reward_offsets[]={0x54,0x5c,0x60};for(size_t reward=0;reward<3;++reward)if(!at(owner,reward_offsets[reward],row.rewards[reward],e))return false;if(!quest_toon(p,quest_owners,owner,row.quest_state,e))return false;out.subscribers.push_back(owner);out.state.subscribers.push_back(row);out.subscriber_slots.push_back(size_t(i));}if(!loans(p,0x90,false,out.loan_data,out.loans,out.state.loans,e)||!loans(p,0xc0,true,out.offer_data,out.offers,out.state.offers,e))return false;if(!canonical_subscribers(out,e)||!valid(out.state,e)||!finish_motion_pass(p,motion_pass,e))return false;if(npc_owners)*npc_owners=std::move(quest_owners);return true;}
bool NativePort::read(Owners& out,std::string& e){return read_with_npc_owners(out,e,nullptr);}
bool NativePort::read_with_npc_owners(Owners& out,std::string& e,std::vector<QuestOwner>* npc_owners){apply_timing::Scope measured(apply_timing::Part::EconomyRead);Owners a,b;std::vector<QuestOwner> last_owners;if(!once(a,e)||!once(b,e,npc_owners?&last_owners:nullptr))return false;if(a.history.pin!=b.history.pin||a.history.owners!=b.history.owners||a.subscriber_manager!=b.subscriber_manager||a.subscriber_data!=b.subscriber_data||a.loan_data!=b.loan_data||a.offer_data!=b.offer_data||a.subscribers!=b.subscribers||a.subscriber_slots!=b.subscriber_slots||a.needs_owners!=b.needs_owners||a.loans!=b.loans||a.offers!=b.offers||a.state!=b.state||a.next_id!=b.next_id)return fail(e,"economic actual owned arrays/values changed during double read");out=std::move(a);if(npc_owners)*npc_owners=std::move(last_owners);e.clear();return true;}
bool NativePort::motion_once(MotionRead& out,std::string& e){
    apply_timing::PageReadScope pages(!apply_timing::page_read_scope);out={};MotionSamplePass pass;
    if(!history_.read(out.history,e))return false;
    const auto& p=out.history.pin;
    if(p.context.session!=actual_day_receipt_.frame.session||p.context.snapshot_uuid!=actual_day_receipt_.frame.snapshot||
       p.common_epoch!=actual_day_receipt_.frame.epoch||p.values!=actual_day_receipt_.frame.values||
       out.history.records.size()!=actual_day_receipt_.history.size())return fail(e,"economic motion actual Day receipt differs");
    for(size_t i=0;i<out.history.records.size();++i)
        if(out.history.records[i]!=actual_day_receipt_.history[i].record)return fail(e,"economic motion actual History differs from retained Day allocation receipt");
    int32_t count=-1,capacity=-1;
    if(!at(p.image,0x13df690,out.subscriber_manager,e)||!ap(out.subscriber_manager,p.image,0x1725f40,e)||
       !at(out.subscriber_manager,0x40,out.subscriber_data,e)||!at(out.subscriber_manager,0x48,count,e)||
       !at(out.subscriber_manager,0x4c,capacity,e)||count<0||capacity<count||count>1000000||
       (count&&(!out.subscriber_data||out.subscriber_data>UINTPTR_MAX-size_t(count)*8)))
        return fail(e,e.empty()?"economic motion actual Subscriber manager bounds differ":e);
    if(!at(out.subscriber_manager,0x68,out.next_id,e))return false;
    if(out.next_id<0)return fail(e,"economic motion native nextID invalid");
    struct Entry {MotionRead::Row row;uintptr_t owner;size_t slot;subscriber_needs::Owner needs;};
    std::vector<Entry> entries;
    for(int32_t i=0;i<count;++i){
        Entry entry{};entry.slot=size_t(i);
        if(!at(out.subscriber_data,entry.slot*8,entry.owner,e)||!ap(entry.owner,p.image,0x16e75f0,e)||
           !at(entry.owner,0x20,entry.row.serialized_id,e))return false;
        if(entry.row.serialized_id<0||entry.row.serialized_id>=out.next_id)return fail(e,"economic motion Subscriber ID outside nextID");
        subscriber_needs::Bits bits;
        if(!subscriber_needs::sample([](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);},
            p.image,entry.owner,entry.row.serialized_id,entry.needs,bits,e)||
           !motion_read(p,entry.owner,entry.row.motion,e,nullptr,nullptr,&pass))return false;
        entries.push_back(std::move(entry));
    }
    std::sort(entries.begin(),entries.end(),[](const Entry& a,const Entry& b){return a.row.serialized_id<b.row.serialized_id;});
    int32_t previous=-1;
    for(auto& entry:entries){
        if(entry.row.serialized_id<=previous)return fail(e,"economic motion canonical Subscriber ID duplicate");
        previous=entry.row.serialized_id;out.rows.push_back(std::move(entry.row));out.subscribers.push_back(entry.owner);
        out.subscriber_slots.push_back(entry.slot);out.needs_owners.push_back(entry.needs);
    }
    auto finance_owners=[&](size_t offset,bool offer,uintptr_t& data,std::vector<uintptr_t>& owners){
        int32_t size=-1,available=-1;
        if(!at(p.business,offset+8,data,e)||!at(p.business,offset+0x10,size,e)||!at(p.business,offset+0x14,available,e))return false;
        if(size<0||available<size||size>1024||(size&&(!data||data>UINTPTR_MAX-size_t(size)*8)))
            return fail(e,"economic motion native Loan/Offer bounded array differs");
        for(int32_t i=0;i<size;++i){uintptr_t owner=0;
            if(!at(data,size_t(i)*8,owner,e)||!ap(owner,p.image,offer?0x16df370:0x17438e0,e))return false;
            owners.push_back(owner);
        }
        return true;
    };
    if(!finance_owners(0x90,false,out.loan_data,out.loans)||!finance_owners(0xc0,true,out.offer_data,out.offers))return false;
    return finish_motion_pass(p,pass,e);
}
bool NativePort::read_motion(MotionRead& out,std::string& e){
    apply_timing::PageReadScope pages;
    out={};MotionRead a,b;
    if(!motion_once(a,e)||!motion_once(b,e))return false;
    if(a.history.pin!=b.history.pin||a.history.owners!=b.history.owners||a.history.records!=b.history.records||
       a.subscriber_manager!=b.subscriber_manager||a.subscriber_data!=b.subscriber_data||
       a.loan_data!=b.loan_data||a.offer_data!=b.offer_data||a.subscribers!=b.subscribers||
       a.subscriber_slots!=b.subscriber_slots||a.needs_owners!=b.needs_owners||a.loans!=b.loans||
       a.offers!=b.offers||a.rows!=b.rows||a.next_id!=b.next_id)
        return fail(e,"economic motion actual owned arrays/poses changed during double read");
    out=std::move(a);e.clear();return true;
}

bool NativePort::current(const Owners& before,uintptr_t owner,bool subscriber,std::string& e,std::vector<QuestOwner>* npc_owners){Owners current;std::vector<QuestOwner> last_owners;if(!read_with_npc_owners(current,e,npc_owners?&last_owners:nullptr))return false;const auto& p=current.history.pin;const auto& old=before.history.pin;if(p.context!=old.context||p.binding!=old.binding||p.world!=old.world||p.clock!=old.clock||p.business!=old.business||p.record!=old.record||p.history!=old.history||p.history_count!=old.history_count||current.history.owners!=before.history.owners||current.subscriber_manager!=before.subscriber_manager||p.common_epoch!=old.common_epoch||p.selected!=0||p.context.role!=action_session::Role::Client||current.subscribers!=before.subscribers||current.subscriber_slots!=before.subscriber_slots||current.needs_owners!=before.needs_owners||current.loans!=before.loans||current.offers!=before.offers||current.subscriber_data!=before.subscriber_data||current.loan_data!=before.loan_data||current.offer_data!=before.offer_data||!fence_.read_current_fence(p,e))return fail(e,e.empty()?"economic current actual Client owner/fence differs":e);if(owner){const auto& list=subscriber?current.subscribers:current.loans;if(std::find(list.begin(),list.end(),owner)==list.end())return fail(e,"economic scalar owner absent from actual manager");}if(npc_owners)*npc_owners=std::move(last_owners);return true;}
bool NativePort::current_npc_owners(const Owners& before,std::vector<QuestOwner>& owners,std::string& e){
    apply_timing::Scope measured(apply_timing::Part::EconomyRead);
    // Resolve native NPC identities before the pure double ownership read.
    // Original Find calls cannot leave a retained pre-callback owner receipt.
    std::vector<QuestOwner> fresh;
    if(!npc_probe::action_runtime::state_owners(before.history.pin.context,before.history.pin.binding,fresh,e))return false;
    MotionRead current;if(!read_motion(current,e))return false;
    const auto& p=current.history.pin;const auto& old=before.history.pin;if(p.context!=old.context||p.binding!=old.binding||p.world!=old.world||p.clock!=old.clock||p.business!=old.business||p.record!=old.record||p.history!=old.history||p.history_count!=old.history_count||current.history.owners!=before.history.owners||current.subscriber_manager!=before.subscriber_manager||p.common_epoch!=old.common_epoch||p.selected!=0||p.context.role!=action_session::Role::Client||current.subscribers!=before.subscribers||current.subscriber_slots!=before.subscriber_slots||current.needs_owners!=before.needs_owners||current.loans!=before.loans||current.offers!=before.offers||current.subscriber_data!=before.subscriber_data||current.loan_data!=before.loan_data||current.offer_data!=before.offer_data||!fence_.read_current_fence(p,e))return fail(e,e.empty()?"economic current actual Client owner/fence differs":e);
    owners=std::move(fresh);e.clear();return true;
}

bool NativePort::preflight_from_read(const Owners& before,const State& target,std::string& e){
    if(!admit_existing(before,target,e))return false;
    const auto& p=before.history.pin;
    if(p.selected!=0||p.context.role!=action_session::Role::Client||p.thread!=GetCurrentThreadId())return fail(e,"economic local read requires actual Client/Mainthread");
    if(!fence_.read_current_fence(p,e))return false;
    // The local receipt was consumed before region_at/login_empty/capacity.
    // A complete read still separates these native queries from the first store.
    if(!preflight_lifecycle_body(before,target,e))return false;
    bool lifecycle_query=false;
    for(size_t i=0;i<target.subscribers.size();++i)if(before.state.subscribers[i].motion.has_value()!=target.subscribers[i].motion.has_value()){lifecycle_query=true;break;}
    if(lifecycle_query&&!current(before,0,true,e))return false;
    return preflight_motion_body(before,target,e);
}
bool NativePort::begin_scalar_stores(const Owners& before,std::string& e){
    if(scalar_before_)return fail(e,"economic scalar phase is already active; no reentrant batch");
    NativeHistory actual;if(!history_.read(actual,e))return false;
    if(actual.pin!=before.history.pin||actual.owners!=before.history.owners||actual.records!=before.history.records)return fail(e,"economic scalar phase complete History/finance receipt differs");
    if(!fence_.read_current_fence(actual.pin,e))return false;
    scalar_history_=std::move(actual);scalar_before_=&before;e.clear();return true;
}
void NativePort::end_scalar_stores()noexcept{scalar_before_=nullptr;scalar_history_={};}
bool NativePort::current_subscriber(const Owners& before,size_t index,std::string& e){
    std::optional<apply_timing::Scope> quest_measured;
    if(apply_timing::quest_store_depth)quest_measured.emplace(apply_timing::Part::QuestSubscriberGuard);
    apply_timing::PageReadScope pages;
    auto read=[](uintptr_t base,size_t offset,auto& value,std::string& error){return at(base,offset,value,error);};
    if(quest_batch_&&subscriber_lifecycle_append::quest_edits_held(*quest_batch_)){
        NativeHistory actual;
        if(!history_.read_quest_held(actual,*quest_batch_,e)||!subscriber_store_owner(read,before,actual,index,e))return false;
        return fence_.read_current_fence(actual.pin,e);
    }
    const bool quest_cached=quest_before_&&quest_owners_ready_;
    if(scalar_before_||quest_cached){
        if(scalar_before_?scalar_before_!=&before:quest_before_!=&before)
            return fail(e,scalar_before_?"economic scalar phase expected owner receipt differs":"Quest scalar phase expected owner receipt differs");
        const auto& retained=scalar_before_?scalar_history_:quest_history_;
        const auto& p=retained.pin;
        uintptr_t image=0;uint32_t thread=0;std::string sha;
        if(!native::read_resolver_receipt(image,thread,sha,e))return false;
        if(image!=p.image||thread!=p.thread||thread!=GetCurrentThreadId()||image!=uintptr_t(GetModuleHandleW(nullptr))||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")return fail(e,"economic scalar actual module/Mainthread differs");
        action_session::Context context;if(!action_session::selected_session().current(context,e)||context!=p.context)return fail(e,e.empty()?"economic scalar actual session differs":e);
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,e))return false;
        if(uintptr_t(world.world)!=p.world||!world.state||world.world_epoch!=p.binding.native_epoch||world.generation!=p.binding.native_generation||net::current_world_epoch()!=p.common_epoch)return fail(e,"economic scalar actual World/epoch/generation differs");
        native::WorldStatus status;if(!native::read_world_status(status,e))return false;
        if(status.saves_in_progress||status.edit_operations)return fail(e,"economic scalar phase requires native Save/EditIdle");
        if(!scalar_store_roots(read,p,e)||!subscriber_store_owner(read,before,retained,index,e))return false;
        return fence_.read_current_fence(p,e);
    }
    NativeHistory actual;if(!history_.read(actual,e))return false;
    if(!subscriber_store_owner(read,before,actual,index,e))return false;
    return fence_.read_current_fence(actual.pin,e);
}
bool NativePort::store_activity(const Owners& before,size_t index,size_t field,uint32_t old,uint32_t target,std::string& e){
    if(index>=before.subscribers.size()||field>=3)return fail(e,"Subscriber activity CAS owner/index invalid");
    if(field==2&&(!subscriber_activity::valid_time(old)||!subscriber_activity::valid_time(target)))return fail(e,"Subscriber activity CAS time pattern invalid");
    if(!current_subscriber(before,index,e))return false;
    return compare_fixed_i32(before.subscribers[index]+subscriber_activity::offsets[field],std::bit_cast<int32_t>(old),std::bit_cast<int32_t>(target),e);
}
bool NativePort::store_need(const Owners& before,size_t index,size_t field,uint32_t old,uint32_t target,std::string& e){
    if(index>=before.subscribers.size()||index>=before.needs_owners.size()||field>=7||!subscriber_needs::valid(old)||!subscriber_needs::valid(target))return fail(e,"Subscriber Needs CAS owner/index/float pattern invalid");
    if(!current_subscriber(before,index,e))return false;
    return compare_fixed_i32(before.needs_owners[index].needs+0x18+field*4,std::bit_cast<int32_t>(old),std::bit_cast<int32_t>(target),e);
}
bool NativePort::store_next_id(const Owners& before,int32_t target,std::string& e){
    if(before.next_id<0||target<=before.next_id)return fail(e,"Subscriber nextID CAS requires monotonic advance actual="+std::to_string(before.next_id)+" required="+std::to_string(target));
    if(!current(before,0,true,e))return false;
    return compare_fixed_i32(before.subscriber_manager+0x68,before.next_id,target,e);
}
bool NativePort::store_subscriber(const Owners& before,size_t index,size_t field,int32_t old,int32_t target,std::string& e){if(index>=before.subscribers.size()||field>=6)return fail(e,"economic Subscriber fixed slot invalid");const auto owner=before.subscribers[index];if(!current_subscriber(before,index,e))return false;return compare_fixed_i32(owner+sub_fields[field],old,target,e);}
bool NativePort::store_loan(const Owners& before,size_t index,size_t field,int32_t old,int32_t target,std::string& e){if(index>=before.loans.size()||field>=4)return fail(e,"economic Loan fixed slot invalid");const auto owner=before.loans[index];if(!current(before,owner,false,e))return false;return compare_fixed_i32(owner+0x78+field*4,old,target,e);}
bool NativePort::subscriber_retirements(const Owners& actual,std::vector<mt2mp::subscriber_lifecycle::additions::Retirement>& out,std::string& e){
    out.clear();if(!subscriber_retirement::bind(actual,e))return false;
    std::vector<std::pair<int32_t,int32_t>> rows;if(!subscriber_retirement::facts(rows,e))return false;
    for(const auto& [id,reason]:rows)out.push_back({id,reason});return true;
}
bool NativePort::retire_subscriber(int32_t id,int32_t reason,nlohmann::json& receipt,std::string& e){
    receipt={{"attempted",false},{"confirmed",false},{"unknown",false}};
    Owners actual;if(!read(actual,e)||!fence_.read_current_fence(actual.history.pin,e))return false;
    auto find=[&](){return std::find_if(actual.state.subscribers.begin(),actual.state.subscribers.end(),[&](const auto& row){return row.serialized_id==id;});};
    auto found=find();if(found==actual.state.subscribers.end())return fail(e,"Subscriber retirement actual saved ID absent");
    if(found->motion){const auto index=size_t(found-actual.state.subscribers.begin());const auto old=found->motion;
        receipt["attempted"]=true;if(!store_lifecycle(actual,index,old,std::nullopt,e)||!read(actual,e)){receipt["unknown"]=true;return false;}found=find();
        if(found==actual.state.subscribers.end()||found->motion){receipt["unknown"]=true;return fail(e,"Subscriber retirement original offline transition readback Unknown");}
    }
    if(!subscriber_retirement::bind(actual,e)){receipt["unknown"]=receipt["attempted"];return false;}
    nlohmann::json removed;const bool ok=subscriber_retirement::apply(id,reason,removed,e);
    const bool offline_attempted=receipt.value("attempted",false);receipt=std::move(removed);if(offline_attempted&&!ok){receipt["attempted"]=true;receipt["unknown"]=true;}
    if(!ok)return false;
    Owners after;if(!read(after,e)){receipt["confirmed"]=false;receipt["unknown"]=true;return false;}
    if(std::any_of(after.state.subscribers.begin(),after.state.subscribers.end(),[&](const auto& row){return row.serialized_id==id;})){receipt["confirmed"]=false;receipt["unknown"]=true;return fail(e,"Subscriber retirement fresh canonical census still contains ID");}
    receipt["fresh_economy_census_confirmed"]=true;return true;
}

bool NativePort::capture_subscriber(int32_t id,nlohmann::json& graph,std::string& e){
    Owners before;if(!read(before,e))return false;const auto& p=before.history.pin;
    building_mixed_identity::opened_file::CurrentWorld w;if(!native_open_runtime::read_current_world(w,e))return false;
    if(p.context.role!=action_session::Role::Host||reinterpret_cast<uintptr_t>(w.world)!=p.world||w.world_epoch!=p.binding.native_epoch||w.generation!=p.binding.native_generation)return fail(e,"automatic Subscriber Host capture owner differs");
    nlohmann::json receipt;if(!subscriber_lifecycle_capture::capture_frame(id,w.uuid,p.context,graph,receipt,e))return false;
    graph["frame_binding"]={{"session",p.context.session},{"snapshot_uuid",p.context.snapshot_uuid},{"epoch",p.common_epoch}};
    Owners after;if(!read(after,e))return fail(e,"automatic Subscriber post-capture read id="+std::to_string(id)+" original="+e);
    if(!restore_capture_pause(before,after,
        [&](const Pin& pin,std::string& error){
            constexpr std::array<unsigned char,10> expected{0x56,0x53,0x48,0x83,0xec,0x48,0x0f,0x2f,0x49,0x5c};
            std::array<unsigned char,10> actual{};
            if(!memory.copy(pin.image+0x259b70,actual.data(),actual.size(),error))return false;
            if(actual!=expected)return fail(error,"Subscriber capture pause original setter bytes differ");
            const auto exception=call_capture_pause(pin.image+0x259b70,pin.clock,pin.pause);
            if(exception)return fail(error,"Subscriber capture pause restore original setter exception="+std::to_string(exception)+"; no replay");
            return true;
        },[&](Owners& restored,std::string& error){return read(restored,error);},e))return fail(e,"automatic Subscriber capture changed actual Frame cohort/scalars id="+std::to_string(id)+
        " save_scope_closed="+std::to_string(receipt.at("save_scope").at("closed_readback_confirmed").get<bool>())+" original="+e);
    return true;
}
bool NativePort::append_subscriber(const nlohmann::json& graph,nlohmann::json& receipt,std::string& e){
    Owners before;if(!read(before,e))return false;const auto& p=before.history.pin;
    building_mixed_identity::opened_file::CurrentWorld w;if(!native_open_runtime::read_current_world(w,e))return false;
    if(p.context.role!=action_session::Role::Client||reinterpret_cast<uintptr_t>(w.world)!=p.world||w.world_epoch!=p.binding.native_epoch||w.generation!=p.binding.native_generation)return fail(e,"automatic Subscriber Client append owner differs");
    if(!fence_.read_current_fence(p,e))return false;
    const subscriber_lifecycle_append::FrameBinding frame{p.context,p.binding,p.common_epoch,w.sha256};
    return subscriber_lifecycle_append::apply_frame(graph,w.uuid,frame,receipt,e);
}

}
