#include "../src/game/live_host_state/memory_query_timing.h"
#include "subscriber_quest_state.h"
#include "npc_quest_visitor_reader.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
namespace quest_state=mt2mp::subscriber_quest_state;
unsigned checks=0,collections=0,reads=0,census_reads=0,callbacks=0,stores=0;
unsigned pointer_reads=0,pointer_headers=0;std::string pointer_fault;
unsigned owner_revision=0,fail_store=0,ensures=0,binds=0,retires=0;
std::string helper_failure;bool change_later_link=false,callback_owner_failure=false;
bool strict_reader=false;
std::string validation_fault;
unsigned validation_headers=0,validation_pointers=0;
uintptr_t progress_toon=0;std::vector<uintptr_t> progress_pointers;
int32_t progress_count=1;
std::vector<uintptr_t> cas_addresses;
bool root_ok=true,owner_ok=true,throw_owner=false,read_ok=true,cas_ok=true,final_bad=false;
bool active_during_callback=false;
std::vector<std::string> events;
std::map<uintptr_t,std::vector<unsigned char>> bytes;
std::map<uintptr_t,quest_state::Toon> toons;
namespace apply_timing=mt2mp::game::live_host_state::apply_timing;
unsigned char* metadata_page=nullptr;
unsigned field_copies=0;
bool protect_on_pause=false,metadata_during_callback=false;
std::string* failure_context=nullptr;
void need(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(std::string(why)+(failure_context?" original="+*failure_context:""));}
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>void put(uintptr_t p,size_t offset,const T& value){auto& out=bytes[p+offset];out.resize(sizeof value);std::memcpy(out.data(),&value,sizeof value);}
template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& e){
    ++field_copies;
    if(metadata_page){
        MEMORY_BASIC_INFORMATION page{};
        if(!apply_timing::virtual_query(metadata_page,&page,sizeof page,apply_timing::Part::QueryPlacement)||
           page.State!=MEM_COMMIT||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS)))
            return fail(e,"original fixture page unreadable");
    }
    if(p==3000)++census_reads;
    if(p==7000||(offset==0&&std::find(progress_pointers.begin(),progress_pointers.end(),p)!=progress_pointers.end()))++pointer_reads;
    auto f=bytes.upper_bound(p+offset);if(f==bytes.begin())return fail(e,"original unreadable fixture field");--f;
    const auto delta=p+offset-f->first;
    if(delta>f->second.size()||sizeof value>f->second.size()-delta)return fail(e,"original unreadable fixture field");
    std::memcpy(&value,f->second.data()+delta,sizeof value);return true;
}
bool ap(uintptr_t p,uintptr_t image,uintptr_t expected,std::string& e){uintptr_t v=0;return at(p,0,v,e)&&(v==image+expected||fail(e,"original class guard"));}
namespace action_session {struct Context {int id=7;bool operator==(const Context&)const=default;};struct Session {bool current(Context& out,std::string& e){out={};return root_ok||fail(e,"original session failure");}};Session& selected_session(){static Session s;return s;}}
struct Binding {uint64_t native_epoch=2,native_generation=3;bool operator==(const Binding&)const=default;};
struct Pin {uintptr_t image=100000000,world=99,clock=4000;DWORD thread=GetCurrentThreadId();action_session::Context context;Binding binding;uint64_t common_epoch=4;float selected=0,pause=1;bool operator==(const Pin&)const=default;};
namespace building_mixed_identity::opened_file {struct CurrentWorld {uintptr_t world=99;uintptr_t state=1;uint64_t world_epoch=2,generation=3;std::string uuid="world",sha256="sha";};}
namespace native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out={};return true;}}
namespace net {uint64_t current_world_epoch(){return 4;}}
namespace native {struct WorldStatus {int saves_in_progress=0,edit_operations=0;};
WorldStatus current_status;
bool resolver_ok=true;uintptr_t resolver_image=uintptr_t(GetModuleHandleW(nullptr));
bool read_resolver_receipt(uintptr_t& image,uint32_t& thread,std::string& sha,std::string& e){image=resolver_image;thread=GetCurrentThreadId();sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";return resolver_ok||fail(e,"original resolver failure");}
bool read_world_status(WorldStatus& out,std::string&){out=current_status;return true;}}
struct QuestOwner {std::string key="npc";uintptr_t npc=6000;std::vector<std::pair<int32_t,uintptr_t>> quests;};
std::vector<QuestOwner> owner_catalog;
namespace npc_probe::action_runtime {bool state_owners(const action_session::Context&,const Binding&,std::vector<QuestOwner>& out,std::string& e){++collections;events.push_back("owners");if(throw_owner)throw std::runtime_error("original collection exception");if(!owner_ok)return fail(e,"original collection failure");out=owner_catalog;if(out[0].key=="npc")out[0].npc+=owner_revision;return true;}}
struct NativeHistory {Pin pin;std::vector<int> owners,records;};
bool phase_tasks_pending=false,phase_weapon_queues_tasks=false;
namespace subscriber_lifecycle_append {
struct QuestEditBatch {bool held=false;};
bool quest_edits_held(const QuestEditBatch& b)noexcept{return b.held;}
bool verify_quest_edits_held(QuestEditBatch& b,std::string& e){
    if(phase_tasks_pending)return fail(e,"Subscriber append original TaskManager not drained");
    return (b.held&&root_ok)||fail(e,"original owned held proof failure");
}
}
bool phase_simulate=false,phase_exit_failure=false,phase_close_failure=false,phase_history_on_exit=false;
unsigned phase_enters=0,phase_leaves=0;
struct State {struct Row {int32_t serialized_id=0;std::optional<quest_state::Toon> quest_state;};std::vector<Row> subscribers;};
struct Owners {NativeHistory history;uintptr_t subscriber_manager=2000,subscriber_data=3000;std::vector<uintptr_t> subscribers;std::vector<size_t> subscriber_slots;State state;};
class NativePort {
public:
    const Owners* scalar_before_=nullptr;
    NativeHistory scalar_history_;
    bool current_subscriber_checked(const Owners&,size_t,std::string&);
    const Owners* quest_before_=nullptr;
    subscriber_lifecycle_append::QuestEditBatch* quest_batch_=nullptr;
    bool with_quest_stores(const Owners&,const State&,const std::function<bool(std::string&)>&,std::string&);
    bool quest_owners_ready_=false;
    NativeHistory quest_history_;
    struct History {unsigned reads=0;bool okay=true,changed=false;NativeHistory expected;
        bool read(NativeHistory& out,std::string& e){++reads;if(!okay)return fail(e,"original History read failure");out=expected;if(changed)out.records.push_back(123);return true;}
        bool read_quest_held(NativeHistory& out,subscriber_lifecycle_append::QuestEditBatch& batch,std::string& e){
            if(!subscriber_lifecycle_append::verify_quest_edits_held(batch,e)||!read(out,e))return false;
            out.pin.pause=0;return true;
        }
    } history_;
    struct Fence {bool okay=true;bool read_current_fence(const Pin&,std::string& e){return okay||fail(e,"original History fence failure");}} fence_;
    bool admit_quest_history(const Owners&,std::string&);
    std::vector<QuestOwner> quest_owners_;
    void clear_quest_span()noexcept;
    bool begin_quest_stores(const Owners&,std::string&);
    void end_quest_stores()noexcept;
    bool store_quest_state(const Owners&,size_t,const std::optional<quest_state::Toon>&,const std::optional<quest_state::Toon>&,std::string&);
    bool store_quest_state_original(const Owners&,size_t,const std::optional<quest_state::Toon>&,const std::optional<quest_state::Toon>&,std::string&);
    bool store_quest_state_membership_previous(const Owners&,size_t,const std::optional<quest_state::Toon>&,const std::optional<quest_state::Toon>&,std::string&);
    bool store_quest_state_metadata_previous(const Owners&,size_t,const std::optional<quest_state::Toon>&,const std::optional<quest_state::Toon>&,std::string&);
    bool current_subscriber(const Owners&,size_t,std::string& e){events.push_back("current");return root_ok||fail(e,"original live root failure");}
};
NativePort* active=nullptr;
using Port=NativePort;
void callback(){++callbacks;++owner_revision;if(callback_owner_failure)owner_ok=false;events.push_back("callback");active_during_callback|=active&&(active->quest_owners_ready_||!active->quest_history_.records.empty());metadata_during_callback|=apply_timing::page_read_scope!=nullptr;}
struct Memory {bool copy(uintptr_t,void* out,size_t count,std::string&){const std::array<unsigned char,10> signature{0x56,0x53,0x48,0x83,0xec,0x48,0x0f,0x2f,0x49,0x5c};if(count!=signature.size())return false;std::memcpy(out,signature.data(),count);return true;}} memory;
uint32_t call_capture_pause(uintptr_t,uintptr_t clock,float pause){callback();put(clock,0x5c,pause);if(protect_on_pause){DWORD prior=0;if(!VirtualProtect(metadata_page,4096,PAGE_NOACCESS,&prior))return GetLastError();}return 0;}
struct QuestMemory : mt2mp::npc_census::Memory {
    bool copy(uintptr_t address,void* out,size_t size,std::string& e)override{
        if(strict_reader&&callbacks&&validation_fault=="header"&&address==progress_toon+0xaf0&&++validation_headers==2)
            put(progress_toon,0xb04,progress_count+1);
        if(strict_reader&&callbacks&&validation_fault=="order"&&address>=7000&&address<7000+size_t(progress_count)*8&&++validation_pointers==unsigned(progress_count)+1)
            put(uintptr_t(7000),0,progress_pointers.back());
        auto f=bytes.upper_bound(address);if(f==bytes.begin())return fail(e,"original unreadable fixture field");--f;
        const auto delta=address-f->first;if(delta>f->second.size()||size>f->second.size()-delta)return fail(e,"original unreadable fixture field");
        std::memcpy(out,f->second.data()+delta,size);return true;
    }
};
namespace mt2mp::subscriber_lifecycle {struct SaveScopeReceipt {bool enter_attempted=false,leave_returned=false,unknown=false;};struct ExternalLink {uintptr_t object=0;std::string portable_key;};}
using Fresh=bool(*)(void*,const mt2mp::subscriber_lifecycle::ExternalLink&,std::string&);
namespace npc_quest_visitor_reader {
using ArrayHeader=mt2mp::npc_quest_visitor_reader::ArrayHeader;
bool read_header(QuestMemory& reader_memory,uintptr_t toon,ArrayHeader& out,std::string& e){
    ++pointer_headers;
    if(stores==1&&pointer_fault=="later_slot")put(uintptr_t(7000),8,progress_pointers.front());
    if(stores==1&&pointer_fault=="later_class")put(progress_pointers[1],0,uintptr_t(0));
    if(pointer_headers==2&&!pointer_fault.empty()){
        if(pointer_fault=="ap")put(toon,0xaf0,uintptr_t(0));
        if(pointer_fault=="data")put(toon,0xaf8,uintptr_t(9000));
        if(pointer_fault=="count")put(toon,0xb00,progress_count-1);
        if(pointer_fault=="capacity")put(toon,0xb04,progress_count+1);
        if(pointer_fault=="slot")put(uintptr_t(7000),0,progress_pointers.back());
        if(pointer_fault=="class")put(progress_pointers.front(),0,uintptr_t(0));
        if(pointer_fault=="unreadable")bytes.erase(7000);
    }
    if(strict_reader)return mt2mp::npc_quest_visitor_reader::read_header(reader_memory,toon,out,e);
    out={100000000+0x16ee0e0,7000,progress_count,std::max(1,progress_count)};return true;
}}
void materialize_progress();
void sync_progress();
namespace subscriber_lifecycle_append {
struct FrameBinding {action_session::Context context;Binding binding;uint64_t epoch;std::string sha;};
struct ProgressRequest {uintptr_t toon,npc;std::string key;Fresh fresh;void* user;};struct ProgressReceipt {uintptr_t progress=8000;};
struct QuestLinkRequest {uintptr_t instance,quest;std::string key;Fresh fresh;void* user;};struct QuestLinkReceipt {};
bool retire_progress(const ProgressRequest& request,uintptr_t pointer,const std::string&,const FrameBinding&,ProgressReceipt&,std::string& e){
    ++retires;callback();if(helper_failure=="retire")return fail(e,"original retirement failure");sync_progress();
    auto found=std::find(progress_pointers.begin(),progress_pointers.end(),pointer);if(found==progress_pointers.end())return fail(e,"original missing retirement row");
    auto i=size_t(found-progress_pointers.begin());progress_pointers.erase(found);toons[request.toon].progress.erase(toons[request.toon].progress.begin()+i);materialize_progress();return true;
}
bool ensure_progress(const ProgressRequest& request,const std::string&,const FrameBinding&,ProgressReceipt& receipt,std::string& e){
    ++ensures;callback();if(helper_failure=="ensure")return fail(e,"original ensure failure");sync_progress();
    auto& rows=toons[request.toon].progress;size_t i=0;for(;i<rows.size();++i)if(rows[i].npc.npc_key==request.key)break;
    if(i==rows.size()){quest_state::Progress row;row.npc={quest_state::LinkState::Live,request.key,-1};rows.push_back(row);progress_pointers.push_back(200000+ensures*0x200);}
    receipt.progress=progress_pointers[i];materialize_progress();return true;
}
bool bind_quest_instance(const QuestLinkRequest& request,const std::string&,const FrameBinding&,QuestLinkReceipt&,std::string& e){
    ++binds;callback();if(helper_failure=="bind")return fail(e,"original bind failure");sync_progress();
    const auto found=std::find(progress_pointers.begin(),progress_pointers.end(),request.instance-0x58);
    if(found==progress_pointers.end())return fail(e,"original missing bind row");auto& row=toons[progress_toon].progress[size_t(found-progress_pointers.begin())];row.quest={};
    for(const auto& owner:owner_catalog)for(const auto& [sequence,pointer]:owner.quests)if(pointer==request.quest)row.quest={quest_state::LinkState::Live,owner.key,sequence};
    if(change_later_link&&binds==1&&toons[progress_toon].progress.size()>2)toons[progress_toon].progress.back().quest={};
    materialize_progress();return true;
}
template<class Body,class Guard>bool with_quest_edits(const std::string&,const FrameBinding&,
    Body body,Guard guard,mt2mp::subscriber_lifecycle::SaveScopeReceipt& receipt,std::string& e){
    QuestEditBatch batch;const bool okay=body(batch,e);
    if(!phase_simulate||!batch.held)return okay;
    receipt.enter_attempted=true;
    need(active->quest_batch_==nullptr,"public held batch pointer cleared before original close guard");
    if(phase_history_on_exit)active->history_.changed=true;
    const bool safe=guard(e);
    if(!safe){receipt.unknown=true;return false;}
    ++phase_leaves;batch.held=false;receipt.leave_returned=!phase_close_failure;
    receipt.unknown=phase_close_failure||!okay;
    if(phase_close_failure)return fail(e,"original phase leave failure");
    return okay;
}
template<class Body,class Guard>bool with_quest_edit_row(QuestEditBatch& batch,Body body,Guard guard,std::string& e){
    const bool okay=body(batch,e);return okay&&(!batch.held||guard(e));
}
bool retire_progress(QuestEditBatch& batch,const ProgressRequest& request,uintptr_t pointer,const std::string& origin,const FrameBinding& frame,ProgressReceipt& r,std::string& e){
    if(phase_simulate&&!batch.held){batch.held=true;++phase_enters;put(uintptr_t(4000),0x5c,0.0f);}
    return retire_progress(request,pointer,origin,frame,r,e);
}
bool ensure_progress(QuestEditBatch&,const ProgressRequest& request,const std::string& origin,const FrameBinding& frame,ProgressReceipt& r,std::string& e){return ensure_progress(request,origin,frame,r,e);}
bool bind_quest_instance(QuestEditBatch&,const QuestLinkRequest& request,const std::string& origin,const FrameBinding& frame,QuestLinkReceipt& r,std::string& e){return bind_quest_instance(request,origin,frame,r,e);}
}
namespace toon_weapon_visual {struct Receipt {bool owned=true,confirmed=true;};bool refresh(uintptr_t,Receipt&,std::string&){callback();if(phase_weapon_queues_tasks)phase_tasks_pending=true;return true;}}
bool same_weapon(const quest_state::Toon& a,const quest_state::Toon& b){return a.weapon_ilvl==b.weapon_ilvl&&a.weapon_category==b.weapon_category&&a.weapon_variant_id==b.weapon_variant_id;}
bool store_toon_weapon(const Pin&,uintptr_t toon,const quest_state::Toon& a,const quest_state::Toon& b,const std::function<bool(std::string&)>& fence,std::string& e){
    if(same_weapon(a,b))return true;if(!fence(e))return false;callback();toons[toon].weapon_ilvl=b.weapon_ilvl;toons[toon].weapon_category=b.weapon_category;toons[toon].weapon_variant_id=b.weapon_variant_id;return true;
}
constexpr size_t fixture_offsets[]={0x850,0x820,0x6f8,0x874,0x750,0x778,0x7a0,0x878};
void materialize_progress(){
    const uintptr_t image=100000000;auto& rows=toons[progress_toon].progress;progress_count=int32_t(rows.size());
    put(progress_toon,0xaf0,image+0x16ee0e0);put(progress_toon,0xaf8,uintptr_t(7000));put(progress_toon,0xb00,progress_count);put(progress_toon,0xb04,std::max(1,progress_count));
    for(size_t i=0;i<rows.size();++i){const auto pointer=progress_pointers[i];const auto& row=rows[i];put(uintptr_t(7000),i*8,pointer);
        put(pointer,0,image+0x172d130);put(pointer,0x18,image+0x1711930);put(pointer,0x38,image+0x16f9de0);
        put(pointer,0x58,image+0x17087e0);put(pointer,0x70,image+0x1711c30);put(pointer,0x90,image+0x16f9f20);put(pointer,0x88,int32_t(41));
        uintptr_t npc=0,quest=0;for(const auto& owner:owner_catalog){if(row.npc.npc_key==owner.key&&row.npc.state==quest_state::LinkState::Live)npc=owner.npc;for(const auto& [sequence,p]:owner.quests)if(row.quest.npc_key==owner.key&&row.quest.sequence_id==sequence&&row.quest.state==quest_state::LinkState::Live)quest=p;}
        auto weak=[&](size_t offset,uintptr_t target,bool is_npc){const uintptr_t proxy=target?target+0x1000000:0;put(pointer,offset,target);put(pointer,offset+8,proxy);if(target){put(proxy,0,target+(is_npc?0x20:0x18));put(proxy,8,int32_t(1));put(target,0,image+(is_npc?0x1742980:0x174ac10));put(target,is_npc?0x28:0x20,proxy);}};
        weak(0x40,npc,true);weak(0x98,quest,false);put(pointer,0x50,row.completed);put(pointer,0xa8,row.monsters_defeated);uint32_t flags=(row.visited?1u:0u)|(row.action_performed?0x100u:0u)|(row.complete?0x10000u:0u);put(pointer,0xac,flags);
    }
}
void sync_progress(){std::string e;auto& rows=toons[progress_toon].progress;for(size_t i=0;i<rows.size();++i){auto p=progress_pointers[i];need(at(p,0x50,rows[i].completed,e)&&at(p,0xa8,rows[i].monsters_defeated,e),"fixture keeps existing row scalar effects");}}
void init_toon(uintptr_t toon,const quest_state::Toon& t){toons[toon]=t;std::array<uint32_t,8> v{uint32_t(t.gold),t.xp_bits,uint32_t(t.level),uint32_t(t.quests_completed),t.attributes[0],t.attributes[1],t.attributes[2],t.seconds_to_level_bits};for(size_t i=0;i<v.size();++i)put(toon,fixture_offsets[i],v[i]);progress_toon=toon;progress_pointers.clear();for(size_t i=0;i<t.progress.size();++i)progress_pointers.push_back(4000000+i*0x200);materialize_progress();}
bool quest_toon(const Pin&,const std::vector<QuestOwner>& owners,uintptr_t subscriber,std::optional<quest_state::Toon>& out,std::string& e){
    apply_timing::PageReadScope pages(!apply_timing::page_read_scope);
    ++reads;events.push_back("read");if(!read_ok)return fail(e,"original Toon read failure");if(owners.empty())return fail(e,"missing owner identities");uintptr_t toon=0;if(!at(subscriber,0x80,toon,e))return false;
    if(strict_reader){
        if(callbacks){
            if(validation_fault=="capacity")put(toon,0xb04,int32_t(100001));
            if(validation_fault=="extent")put(toon,0xaf8,UINTPTR_MAX-uintptr_t(7));
            if(validation_fault=="duplicate")put(uintptr_t(7000),8,progress_pointers.front());
            if(validation_fault=="scalar")put(toon,0x6f8,int32_t(-1));
            if(validation_fault=="completed")put(progress_pointers.back(),0x50,int32_t(-2));
            if(validation_fault=="monsters")put(progress_pointers.back(),0xa8,int32_t(-1));
        }
        validation_headers=validation_pointers=0;QuestMemory reader_memory;mt2mp::npc_quest_visitor_reader::State checked;
        auto identify=[&](mt2mp::npc_quest_visitor_reader::Kind kind,uintptr_t pointer,std::string& key,std::string& error){
            for(const auto& owner:owners){
                if(kind==mt2mp::npc_quest_visitor_reader::Kind::Npc&&owner.npc==pointer){key=owner.key;return true;}
                if(kind==mt2mp::npc_quest_visitor_reader::Kind::Quest)for(const auto& [sequence,quest]:owner.quests)
                    if(quest==pointer){key=owner.key+"/quest/"+std::to_string(sequence);return true;}
            }
            return fail(error,"Quest progress target lacks current canonical NPC/Quest binding");
        };
        if(!mt2mp::npc_quest_visitor_reader::read(reader_memory,100000000,subscriber,identify,checked,e))return false;
    }
    auto t=toons[toon];std::array<uint32_t,8> v{};for(size_t i=0;i<v.size();++i)if(!at(toon,fixture_offsets[i],v[i],e))return false;
    t.gold=std::bit_cast<int32_t>(v[0]);t.xp_bits=v[1];t.level=std::bit_cast<int32_t>(v[2]);t.quests_completed=std::bit_cast<int32_t>(v[3]);for(size_t i=0;i<3;++i)t.attributes[i]=v[i+4];t.seconds_to_level_bits=v[7];
    for(size_t i=0;i<t.progress.size();++i){auto pointer=progress_pointers[i];if(!at(pointer,0x50,t.progress[i].completed,e)||!at(pointer,0xa8,t.progress[i].monsters_defeated,e))return false;uint32_t flags=0;if(!at(pointer,0xac,flags,e))return false;t.progress[i].visited=(flags&1)!=0;t.progress[i].action_performed=(flags&0x100)!=0;t.progress[i].complete=(flags&0x10000)!=0;}
    if(final_bad&&stores)t.gold+=1;out=t;return true;
}
bool compare_fixed_i32(uintptr_t address,int32_t old,int32_t value,std::string& e){++stores;cas_addresses.push_back(address);events.push_back("cas");int32_t actual=0;if(!at(address,0,actual,e))return false;if(!cas_ok||actual!=old||stores==fail_store)return fail(e,"original CAS failure");put(address,0,value);return true;}
#include "noop_weak_struct.inc"
#include "noop_row_struct.inc"
#include "noop_weak.inc"
#include "noop_row.inc"
#include "fresh_struct.inc"
#include "fresh.inc"
#include "pause.inc"
#include "begin.inc"
unsigned fresh_roots=0,selected_owners=0;bool roots_ok=true,selected_ok=true;
template<class Read>bool scalar_store_roots(Read&&,const Pin&,std::string& e){++fresh_roots;return roots_ok||fail(e,"original scalar live roots failure");}
template<class Read>bool subscriber_store_owner(Read&&,const Owners& before,const NativeHistory& actual,size_t index,std::string& e){
    ++selected_owners;if(!selected_ok)return fail(e,"original selected owner failure");
    return (index<before.subscribers.size()&&actual.pin.context==before.history.pin.context&&actual.pin.binding==before.history.pin.binding&&actual.pin.common_epoch==before.history.pin.common_epoch&&actual.pin.world==before.history.pin.world&&actual.pin.image==before.history.pin.image&&actual.pin.thread==before.history.pin.thread&&actual.pin.clock==before.history.pin.clock&&actual.pin.selected==before.history.pin.selected&&actual.records==before.history.records&&actual.owners==before.history.owners)||fail(e,"original selected complete receipt differs");
}
#include "current_subscriber.inc"
#include "admit_history.inc"
#include "phase.inc"
#include "clear.inc"
#include "end.inc"
#include "store.inc"
#include "store_original.inc"
#include "store_membership_previous.inc"
#include "store_metadata_previous.inc"
Owners setup(){
    phase_tasks_pending=phase_weapon_queues_tasks=false;
    phase_simulate=phase_exit_failure=phase_close_failure=phase_history_on_exit=false;phase_enters=phase_leaves=0;
    pointer_reads=pointer_headers=0;pointer_fault.clear();strict_reader=false;validation_fault.clear();validation_headers=validation_pointers=0;bytes.clear();toons.clear();events.clear();cas_addresses.clear();owner_catalog={{"npc",6000,{}}};ensures=binds=retires=0;helper_failure.clear();change_later_link=false;callback_owner_failure=false;collections=reads=census_reads=callbacks=stores=owner_revision=fail_store=0;progress_count=1;root_ok=owner_ok=read_ok=cas_ok=true;throw_owner=final_bad=active_during_callback=false;
    Owners o;o.history.records={55};if(active){active->history_={};active->history_.expected=o.history;active->fence_={};}o.subscribers={10000,20000,30000};o.subscriber_slots={0,1,2};o.state.subscribers={{0},{1},{2}};const auto& p=o.history.pin;
    put(p.image,0x13df690,o.subscriber_manager);put(o.subscriber_manager,0,p.image+0x1725f40);put(o.subscriber_manager,0x40,o.subscriber_data);put(o.subscriber_manager,0x48,int32_t(3));put(o.subscriber_manager,0x4c,int32_t(3));
    put(p.image,0x13df9a0,p.clock);put(p.clock,0,p.image+0x17499d0);put(p.clock,0x58,float(0));put(p.clock,0x5c,float(1));
    put(uintptr_t(7000),0,uintptr_t(8000));put(uintptr_t(8000),0,p.image+0x172d130);put(uintptr_t(8000),0x50,int32_t(0));put(uintptr_t(8000),0xa8,int32_t(0));put(uintptr_t(8000),0xac,uint32_t(0));
    for(size_t i=0;i<3;++i){auto subscriber=o.subscribers[i],toon=subscriber+40000;put(o.subscriber_data,i*8,subscriber);put(subscriber,0,p.image+0x16e75f0);put(subscriber,0x20,int32_t(i));put(subscriber,0x80,toon);put(toon,0,p.image+0x1744e90);put(toon,0x940,subscriber);init_toon(toon,{});}
    return o;
}
#include "quest_noop_cases.inc"
#include "quest_phase_cases.inc"
int main(){try{
    quest_phase_cases();
    std::string e;failure_context=&e;NativePort port;active=&port;auto o=setup();quest_state::Toon old,next;next.gold=4;
    need(port.begin_quest_stores(o,e),"begin");
    for(size_t i=0;i<3;++i)need(port.store_quest_state(o,i,old,next,e),"scalar row");
    need(collections==1&&reads==6&&stores==3&&callbacks==0,"three stores share only identities; full live reads retained");
    need(census_reads==6,"one full membership admission plus three selected slots");
    need(events==std::vector<std::string>({"current","owners","read","cas","read","current","read","cas","read","current","read","cas","read"}),"exact scalar admission/read/CAS/final ordering");
    port.end_quest_stores();need(!port.quest_before_&&!port.quest_owners_ready_,"scope clears");
    o=setup();need(port.store_quest_state(o,0,old,next,e)&&port.store_quest_state(o,1,old,next,e),"standalone stores");need(collections==2&&census_reads==6,"standalone keeps original collection and full membership per store");
    o=setup();need(port.begin_quest_stores(o,e),"begin mixed");need(port.store_quest_state(o,0,old,next,e),"prime mixed");auto weapon=next;weapon.weapon_ilvl=2;need(port.store_quest_state(o,1,old,weapon,e),"weapon row");need(!port.quest_owners_ready_&&!active_during_callback&&callbacks==2,"Property.Load and Equip occur outside span");need(port.store_quest_state(o,2,old,next,e)&&collections==3,"fresh owners after weapon callback");port.end_quest_stores();
    o=setup();need(port.begin_quest_stores(o,e),"begin pause");put(o.history.pin.clock,0x5c,float(0));need(port.store_quest_state(o,0,old,next,e),"actual pause restoration");need(callbacks==1&&!active_during_callback&&!port.quest_owners_ready_,"span ends before actual setter");need(port.store_quest_state(o,1,old,next,e)&&collections==2,"reacquire after pause setter");port.end_quest_stores();
    for(int lifetime=0;lifetime<2;++lifetime){o=setup();need(port.begin_quest_stores(o,e),"begin Progress boundary");need(port.store_quest_state(o,0,old,next,e),"prime Progress span");auto with_progress=old;with_progress.progress.push_back({});
        if(lifetime)with_progress.progress[0].npc={quest_state::LinkState::Live,"npc",-1};
        init_toon(60000,with_progress);auto changed=with_progress;changed.gold=4;if(lifetime)changed.progress.clear();else changed.progress[0].completed=1;
        need(port.store_quest_state(o,1,with_progress,changed,e),"changed Progress original path");need(port.quest_owners_ready_==!bool(lifetime)&&!active_during_callback&&callbacks==unsigned(lifetime),"only Progress lifetime changes close span; scalar rows retain it");
        need(port.store_quest_state(o,2,old,next,e)&&collections==(lifetime?3u:1u),"reacquire after lifetime change; reuse after Progress scalars");need(port.quest_owners_[0].npc==6000+owner_revision,"reacquired owner reflects callback change");port.end_quest_stores();}
    o=setup();auto all=old;all.gold=1;all.xp_bits=2;all.level=3;all.quests_completed=4;all.attributes={5,6,7};all.seconds_to_level_bits=8;
    need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,all,e),"all eight scalar fields");std::vector<uintptr_t> expected_addresses;for(auto offset:fixture_offsets)expected_addresses.push_back(50000+offset);need(cas_addresses==expected_addresses,"all eight original CAS addresses and order retained");port.end_quest_stores();
    o=setup();fail_store=4;need(port.begin_quest_stores(o,e),"begin partial CAS failure");need(!port.store_quest_state(o,0,old,all,e)&&e=="original CAS failure"&&stores==4,"failed fourth CAS stops later effects");uint32_t live=0;need(at(uintptr_t(50000),fixture_offsets[2],live,e)&&live==3,"earlier successful CAS remains visible");need(at(uintptr_t(50000),fixture_offsets[3],live,e)&&live==0,"failed CAS leaves original field");port.end_quest_stores();
    for(int fault=0;fault<6;++fault){o=setup();need(port.begin_quest_stores(o,e),"begin faults");need(port.store_quest_state(o,0,old,next,e),"prime faults");auto before_stores=stores;
        if(fault==0)root_ok=false;if(fault==1)put(uintptr_t(60000),0x850,uint32_t(9));if(fault==2)read_ok=false;if(fault==3)cas_ok=false;if(fault==4)put(o.subscriber_data,8,uintptr_t(10000));if(fault==5)final_bad=true;
        need(!port.store_quest_state(o,1,old,next,e),"fault fails");
        const char* causes[]={"original live root failure","Quest apply prior Toon state changed","original Toon read failure","original CAS failure","Quest apply reciprocal Subscriber/Toon ownership differs","Quest apply prior Toon state changed"};
        need(e==causes[fault],"original cause retained");if(fault!=3)need(stores==before_stores,"admission failure precedes effects");port.end_quest_stores();}
    o=setup();need(port.begin_quest_stores(o,e),"begin final read fault");final_bad=true;need(!port.store_quest_state(o,0,old,next,e)&&e=="Quest apply complete Toon state readback differs"&&stores==1,"post-effect final mismatch retained");port.end_quest_stores();
    o=setup();put(o.subscriber_data,8,uintptr_t(10000));need(port.begin_quest_stores(o,e),"begin duplicate");need(!port.store_quest_state(o,0,old,next,e)&&e=="Quest scalar Subscriber admission is not unique"&&stores==0,"full admission rejects duplicate membership");port.end_quest_stores();
    o=setup();owner_ok=false;need(port.begin_quest_stores(o,e),"begin owner failure");need(!port.store_quest_state(o,0,old,next,e)&&e=="original collection failure"&&!port.quest_owners_ready_,"owner failure publishes no span");port.end_quest_stores();
    o=setup();throw_owner=true;bool caught=false;try{need(port.begin_quest_stores(o,e),"begin exception");
#include "scope.inc"
        port.store_quest_state(o,0,old,next,e);
    }catch(const std::runtime_error& x){caught=std::string(x.what())=="original collection exception";}
    need(caught&&!port.quest_before_&&!port.quest_owners_ready_,"actual orchestration scope clears on original exception");
    o=setup();need(port.begin_quest_stores(o,e),"begin reentrant");need(!port.begin_quest_stores(o,e),"reject reentrant");auto other=o;need(!port.store_quest_state(other,0,old,next,e)&&stores==0,"reject different owner receipt");port.end_quest_stores();
    o=setup();need(port.begin_quest_stores(o,e),"begin History admission failure");port.history_.okay=false;
    need(!port.store_quest_state(o,0,old,next,e)&&e=="original History read failure"&&!port.quest_owners_ready_&&stores==0,"failed post-Find History never authorizes stores");port.end_quest_stores();
    o=setup();need(port.begin_quest_stores(o,e),"begin History mismatch");port.history_.changed=true;
    need(!port.store_quest_state(o,0,old,next,e)&&e=="Quest scalar phase complete History/finance receipt differs"&&!port.quest_owners_ready_&&stores==0,"changed History receipt rejected before stores");port.end_quest_stores();
    o=setup();need(port.begin_quest_stores(o,e),"begin History fence failure");port.fence_.okay=false;
    need(!port.store_quest_state(o,0,old,next,e)&&e=="original History fence failure"&&!port.quest_owners_ready_&&stores==0,"History admission fence preserved");port.end_quest_stores();
    o=setup();need(port.begin_quest_stores(o,e),"begin retained History");
    for(size_t i=0;i<3;++i)need(port.store_quest_state(o,i,old,next,e),"qualified row shares admitted History");
    need(port.history_.reads==1,"one post-Find complete History admission per callback-free span");
    port.quest_history_.records={7};port.end_quest_stores();need(port.quest_history_.records.empty()&&!port.quest_owners_ready_,"History discarded at phase end");
    // Exercise the actual receipt selection and fresh guard orchestration.
    o=setup();o.history.pin.image=uintptr_t(GetModuleHandleW(nullptr));port.history_.expected=o.history;
    need(port.begin_quest_stores(o,e),"begin actual current subscriber path");
    fresh_roots=selected_owners=0;
    need(port.current_subscriber_checked(o,0,e)&&port.history_.reads==1,"first selected row keeps complete History read");
    need(port.admit_quest_history(o,e),"post-Find History admission");port.quest_owners_ready_=true;
    for(unsigned i=0;i<12;++i)need(port.current_subscriber_checked(o,i%3,e),"later selected rows keep fresh guards");
    need(port.history_.reads==2&&fresh_roots==12&&selected_owners==13,"13 rows use two complete reads and all 13 selected proofs");
    for(unsigned fault=0;fault<7;++fault){
        e.clear();auto alien=o;roots_ok=selected_ok=native::resolver_ok=true;native::current_status={};port.fence_.okay=true;
        if(fault==0)roots_ok=false;if(fault==1)selected_ok=false;if(fault==2)native::resolver_ok=false;
        if(fault==3)native::current_status.saves_in_progress=1;if(fault==4)native::current_status.edit_operations=1;
        if(fault==5)port.fence_.okay=false;
        need(!port.current_subscriber_checked(fault==6?alien:o,1,e),"cached path preserves live guard failure");
        const char* expected[]={"original scalar live roots failure","original selected owner failure","original resolver failure","economic scalar phase requires native Save/EditIdle","economic scalar phase requires native Save/EditIdle","original History fence failure","Quest scalar phase expected owner receipt differs"};
        need(e==expected[fault]&&port.history_.reads==2,"cached path preserves cause without hidden full reread");
    }
    roots_ok=selected_ok=native::resolver_ok=true;native::current_status={};port.fence_.okay=true;
    port.clear_quest_span();need(port.current_subscriber_checked(o,1,e)&&port.history_.reads==3,"callback invalidation restores full History path");
    port.end_quest_stores();need(port.current_subscriber_checked(o,2,e)&&port.history_.reads==4,"outside phase uses original full History path");
    port.scalar_before_=&o;port.scalar_history_=o.history;
    need(port.current_subscriber_checked(o,0,e)&&port.history_.reads==4,"existing scalar receipt path unchanged");port.scalar_before_=nullptr;
    noop_cases(port,e);
    // Run the actual before/after bodies against identical live field reads.
    // The owned page models their shared allocation metadata independently.
    metadata_page=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    need(metadata_page!=nullptr,"owned row metadata page");
    unsigned previous_copies=0;uint64_t previous_queries=0;
    for(int variant=0;variant<2;++variant){
        o=setup();old={};old.progress.resize(3);next=old;next.gold=4;
        for(size_t i=0;i<next.progress.size();++i)next.progress[i].completed=int32_t(i+1);
        init_toon(50000,old);need(port.begin_quest_stores(o,e),"begin row metadata comparison");
        field_copies=0;apply_timing::totals={};
        bool okay=false;
        {apply_timing::Scope measured(apply_timing::Part::Take);
            okay=variant?port.store_quest_state(o,0,old,next,e):port.store_quest_state_metadata_previous(o,0,old,next,e);}
        need(okay&&callbacks==0&&stores==4,"both row bodies retain exact scalar effects without native callbacks");
        const auto queries=apply_timing::totals[size_t(apply_timing::Part::MemoryVirtualQuery)].count;
        if(!variant){previous_copies=field_copies;previous_queries=queries;}
        else{
            need(field_copies==previous_copies,"row metadata reuse retains every live field copy");
            need(queries<previous_queries,"actual row scope reduces real OS metadata queries");
            std::cout<<"Quest row metadata queries: "<<previous_queries<<" -> "<<queries<<"; field copies: "<<field_copies<<" -> "<<field_copies<<"\n";
        }
        need(!apply_timing::page_read_scope,"row metadata ends on return");port.end_quest_stores();
    }
    o=setup();old={};next={};next.gold=4;protect_on_pause=true;metadata_during_callback=false;
    put(o.history.pin.clock,0x5c,float(0));need(port.begin_quest_stores(o,e),"begin row native-boundary mapping change");
    need(!port.store_quest_state(o,0,old,next,e)&&e=="original fixture page unreadable","post-setter read rejects changed page protection");
    need(callbacks==1&&!metadata_during_callback&&!apply_timing::page_read_scope,"row metadata closes before native pause setter");
    protect_on_pause=false;DWORD restored=0;need(VirtualProtect(metadata_page,4096,PAGE_READWRITE,&restored)!=0,"restore owned row metadata page");
    port.end_quest_stores();need(VirtualFree(metadata_page,0,MEM_RELEASE)!=0,"release owned row metadata page");metadata_page=nullptr;
    o=setup();e.clear();old={};next={};next.gold=4;
    apply_timing::totals={};
    {apply_timing::Scope measured(apply_timing::Part::QuestStores);
        need(port.begin_quest_stores(o,e)&&port.store_quest_state(o,0,old,next,e),"timed scalar store retains success");
        auto timed_weapon=next;timed_weapon.weapon_ilvl=2;
        need(port.store_quest_state(o,1,old,timed_weapon,e),"timed nonqualified store retains effects");
        port.end_quest_stores();
    }
    need(apply_timing::totals[size_t(apply_timing::Part::QuestScalarQualified)].count==1&&apply_timing::totals[size_t(apply_timing::Part::QuestNonqualified)].count==1,"exact scalar/nonqualified classification");
    need(apply_timing::totals[size_t(apply_timing::Part::QuestFreshLinkGuard)].count>0&&apply_timing::quest_store_depth==0,"actual extracted fresh guard timed only inside phase");
    need(apply_timing::totals[size_t(apply_timing::Part::QuestAdmitHistory)].count==1,"actual extracted History admission is timed once for scalar span");
    need(apply_timing::totals[size_t(apply_timing::Part::QuestRestorePause)].count==2,"actual extracted pause guard is timed for both completed rows");
    const auto timed_guards=apply_timing::totals[size_t(apply_timing::Part::QuestFreshLinkGuard)].count;
    o=setup();need(port.store_quest_state(o,0,old,next,e),"standalone fresh store remains unchanged");
    need(apply_timing::totals[size_t(apply_timing::Part::QuestFreshLinkGuard)].count==timed_guards&&apply_timing::totals[size_t(apply_timing::Part::QuestNonqualified)].count==1,"standalone work excluded from QuestStores attribution");
    need(apply_timing::totals[size_t(apply_timing::Part::QuestAdmitHistory)].count==1&&apply_timing::totals[size_t(apply_timing::Part::QuestRestorePause)].count==2,"standalone History and pause reads excluded from Quest attribution");
    std::cout<<checks<<" actual Quest scalar batch checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<"\n";return 1;}}
