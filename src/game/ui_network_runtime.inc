#include "mt2mp/hooks/original_body.h"
#include "ui_network_runtime.h"
#include "network_remove_runtime.h"
#include "ui_remove_runtime.h"
#include "network_identity.h"
#include "network_history_runtime.h"
#include "network_admission.h"
#include "connected_network_reader.h"
#include "network_pipe_preview.h"
#include "network_pipe_codeguards.h"
#include "network_node_codeguards.h"
namespace mt2mp::game::typed_action_runtime {bool submit_network_pipe(const std::string&,const action_session::NetworkPipeIntent&,std::string&);bool network_binding(const std::string&,action_session::Binding&,std::string&);bool submit_network(const std::string&,const action_session::NetworkIntent&,std::string&);}
namespace mt2mp::game::ui_network_runtime {
namespace {
namespace action=action_session;namespace region=ui_region_runtime;namespace terrain=ui_terrain_runtime;
namespace reader=connected_network_native;namespace undo=build_actions::private_native;
using json=nlohmann::json;using Press=void(*)(void*,void*,const void*);using Purchase=bool(*)(void*,const void*);
struct Pending{action::NetworkIntent intent;action::Binding binding;std::string snapshot,token;std::optional<action::Context> context;action::NetworkState graph_before;};
struct Operation{action::Binding binding;action::Request request;action::Evidence evidence;uintptr_t created=0;uint64_t uid=0;unsigned calls=0;bool attempted=false,returned=false,confirmed=false,retired=false,ordered=false;std::string cause;};
struct Runtime{bool attempted=false,ready=false,capture_failed=false;Press press=nullptr;Purchase purchase=nullptr;uintptr_t capture_receiver=0;std::optional<Pending> captured,pending;std::map<std::string,Operation> operations;std::map<std::string,json> jobs;uint64_t captures=0,submits=0,rejections=0;std::string token,cause,last_action;};
Runtime state;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
region::Runtime* runtime(std::string& e){return terrain::runtime(e);}
bool ordered_scope(const action::Binding& binding,const action::Request& request,std::string& error){
    if(request.action!=action::Action::PlaceNetworkNode&&request.action!=action::Action::PlaceNetworkPipe)
        return fail(error,"Ordered Network scope requires Node or Pipe construction");
    action::Context context;if(!action::selected_session().current(context,error))return false;
    if(Config::instance().is_host!=(context.role==action::Role::Host))return fail(error,"Ordered Network actual role differs");
    return action::tick_commands::authority::matches(context,binding,request,error);
}
bool execution_admission(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,bool& ordered,std::string& error){
    ordered=action::tick_commands::authority::active();
    if(ordered){
        if(authority)return fail(error,"Ordered Network execution forbids Host result and RNG adoption");
        return ordered_scope(binding,request,error);
    }
    return !typed_action_runtime::controller_status().value("ordered",false)||fail(error,"Ordered Network execution requires consumed command scope");
}
bool input_admission(std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true))return fail(error,"Network input requires started unlocked Controller");
    if(!controller.value("ordered",false))return !controller.value("pending",true)||fail(error,"Network input requires idle legacy Controller");
    bool inside=false;action::Binding binding;
    if(!typed_action_runtime::original_go_admission(inside,error))return false;
    return (inside&&typed_action_runtime::read_binding(net::current_snapshot_id(),binding,error))||
        fail(error,error.empty()?"Ordered Network input requires actual OriginalGo":error);
}
bool capture_context(const std::string& snapshot,const action::Binding& binding,std::optional<action::Context>& out,std::string& error){
    out.reset();if(!typed_action_runtime::controller_status().value("ordered",false))return true;
    action::Context context;action::Binding current;
    if(!input_admission(error)||!action::selected_session().current(context,error)||context.snapshot_uuid!=snapshot||
       Config::instance().is_host!=(context.role==action::Role::Host)||!typed_action_runtime::read_binding(snapshot,current,error)||current!=binding)
        return fail(error,error.empty()?"Ordered Network capture Session/Binding/role differs":error);
    out=context;return true;
}
template<class Pending>bool capture_ready(const Pending& pending,bool& ready,std::string& error){
    ready=false;const auto controller=typed_action_runtime::controller_status();
    if(!pending.context){
        if(controller.value("ordered",false))return fail(error,"Network capture changed from legacy to ordered mode");
        ready=true;return true;
    }
    bool inside=false;action::Context current;action::Binding binding;
    if(!controller.value("ordered",false)||!controller.value("started",false)||controller.value("locked",true)||
       !typed_action_runtime::original_go_admission(inside,error)||!action::selected_session().current(current,error)||
       current!=*pending.context||net::current_snapshot_id()!=pending.snapshot||
       Config::instance().is_host!=(current.role==action::Role::Host)||!typed_action_runtime::read_binding(pending.snapshot,binding,error)||binding!=pending.binding)
        return fail(error,error.empty()?"Ordered Network retained Context/Binding/role changed":error);
    ready=!inside&&!controller.value("pending",true);return true;
}

void rejected(const std::string& e){++state.rejections;if(e!=state.cause){state.cause=e;LOG_ERROR("[UINetwork] Rejected cause={}",e);}}
bool native_error(uint32_t value,const char* name,std::string& e){return !value||fail(e,std::string("network original ")+name+" SEH="+std::to_string(value));}
bool graph(region::Runtime& r,const region::LocalView& view,reader::Graph& graph,action::NetworkState& out,std::string& e){
    if(!reader::graph(r.memory,r.image,view.world.root,graph,e))return false;
    out.nodes=graph.nodes.size();out.pipes=graph.pipes;out.connections=graph.connections;return region::sha256(graph.rows.dump(),out.graph_sha256,e);
}
bool definition(region::Runtime& r,uintptr_t tool,action::NetworkIntent& intent,uintptr_t& result,std::string& e){
    if(!reader::read::ap(r.memory,tool,r.image,0x173ebd0,e)||!region::at(r,tool,0x168,result,e)||!reader::read::ap(r.memory,result,r.image,0x1738010,e)||
       !region::text(r,tool+0x148,intent.type,e)||!region::text(r,tool+0x50,intent.key,e)||!region::at(r,result,0x78,intent.quoted_cost,e))return false;
    std::string type,key;if(!region::text(r,result+0x18,type,e)||!region::text(r,result+0x38,key,e))return false;
    if(type!=intent.type||key!=intent.key||intent.quoted_cost<0)return fail(e,"network tool definition type/key/cost differs");return true;
}
bool catalogue_key(region::Runtime& r,uintptr_t item,std::string& key,std::string& e){
    uintptr_t data=0;uint64_t length=0;
    if(!region::at(r,item,0x68,data,e)||!region::at(r,item,0x70,length,e))return false;
    if(!data||length>256)return fail(e,"network catalogue key span invalid length="+std::to_string(length)+" required=0..256 with nonnull buffer");
    // Unnamed cursor items remain in the original linked catalogue.
    if(!length){key.clear();return true;}
    return region::text(r,item+0x68,key,e);
}
bool catalog(region::Runtime& r,const action::NetworkIntent& request,uintptr_t& tool,uintptr_t& def,std::string& e){
    uintptr_t item=0;if(!region::at(r,r.image,0x13e2050,item,e))return false;std::set<uintptr_t> visited;tool=def=0;
    while(item){if(visited.size()>=4096||!visited.insert(item).second)return fail(e,"network actual item catalogue cycle/bound");
        uintptr_t ap=0;if(!region::at(r,item,0,ap,e))return false;
        if(ap==r.image+0x173b7b0){std::string key;if(!catalogue_key(r,item,key,e))return false;
            if(key==request.key){if(tool)return fail(e,"network requested item key is ambiguous");
                if(!region::at(r,item,0x160,tool,e))return false;action::NetworkIntent actual=request;
                uintptr_t backlink=0;if(!definition(r,tool,actual,def,e)||!region::at(r,tool,0x118,backlink,e))return false;
                if(backlink!=item||actual!=request)return fail(e,"network actual item/tool/definition differs from captured intent");}}
        if(!region::at(r,item,0x30,item,e))return false;
    }
    if(!tool)return fail(e,"network requested actual catalogue tool absent");
    uintptr_t manager=0;std::vector<uintptr_t> defs;
    if(!region::at(r,r.image,0x13df680,manager,e)||!reader::read::ap(r.memory,manager,r.image,0x16ee8c0,e)||!path_native::array(r.memory,manager,8,0x10,0x14,4096,defs,e))return false;
    return std::count(defs.begin(),defs.end(),def)==1||fail(e,"network definition actual manager membership differs");
}
uint32_t preflight(uintptr_t image,uintptr_t tool,uintptr_t cursor,const action::NetworkIntent& intent,uintptr_t& region,float& height,bool& legal){
    __try{legal=reinterpret_cast<bool(*)(void*,void*,const void*)>(image+0x5c5370)(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),intent.position.data());
        auto* map=*reinterpret_cast<void**>(image+0x13df8c0);
        region=uintptr_t(reinterpret_cast<void*(*)(void*,const void*,void*)>(image+0x32a990)(map,intent.position.data(),nullptr));
        height=reinterpret_cast<float(*)(void*,const void*)>(image+0x32ab80)(map,intent.position.data());return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t ui_call(uintptr_t image,unsigned step,uintptr_t tool,uintptr_t cursor,const std::array<float,3>& point){
    __try{if(step==0)reinterpret_cast<void(*)(void*)>(image+0x580870)(reinterpret_cast<void*>(tool));
        else if(step==1)reinterpret_cast<Press>(image+0x5c5450)(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),point.data());
        else if(step==2)state.press(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),point.data());
        else if(step==3)reinterpret_cast<void(*)(void*,void*)>(image+0x5854e0)(reinterpret_cast<void*>(cursor),nullptr);
        else reinterpret_cast<Press>(image+0x5c5580)(reinterpret_cast<void*>(tool),reinterpret_cast<void*>(cursor),point.data());return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void press_entry(void*,void*,const void*);
bool purchase_entry(void*,const void*);
bool code(region::Runtime& r,bool hooked,std::string& e){
    for(size_t i=0;i<std::size(network_node_codes);++i){const auto& g=network_node_codes[i];std::vector<unsigned char> actual(g.size);
        if(!r.memory.copy(r.image+g.rva,actual.data(),actual.size(),e))return false;
        if(!hooked||i>=2){if(g.rva==0x5854e0){if(!hooks::framework::verify_original_bytes(r.image+g.rva,actual.data(),g.bytes,g.size,e))return fail(e,"network original code differs RVA="+std::to_string(g.rva)+" original="+e);}else if(std::memcmp(actual.data(),g.bytes,g.size))return fail(e,"network original code differs RVA="+std::to_string(g.rva));continue;}
        if(actual[0]!=0xe9||std::memcmp(actual.data()+5,g.bytes+5,g.size-5))return fail(e,"network owned hook tail differs");
        const auto original=i?uintptr_t(state.purchase):uintptr_t(state.press);std::vector<unsigned char> prefix(g.span);
        if(!r.memory.copy(original,prefix.data(),prefix.size(),e)||std::memcmp(prefix.data(),g.bytes,g.span))return fail(e,"network original trampoline prefix differs");
        constexpr unsigned char jump[]{0xff,0x25,0,0,0,0};unsigned char bytes[6]{};uintptr_t target=0;
        if(!r.memory.copy(original+g.span,bytes,6,e)||std::memcmp(bytes,jump,6)||!r.memory.copy(original+g.span+6,&target,8,e)||target!=r.image+g.rva+g.span)return fail(e,"network trampoline continuation differs");
        int32_t displacement=0;std::memcpy(&displacement,actual.data()+1,4);const auto relay=uintptr_t(int64_t(r.image+g.rva+5)+displacement);
        if(!r.memory.copy(relay,bytes,6,e)||std::memcmp(bytes,jump,6)||!r.memory.copy(relay+6,&target,8,e)||target!=(i?uintptr_t(&purchase_entry):uintptr_t(&press_entry)))return fail(e,"network hook relay owner differs");
    }return true;
}
bool purchase_entry(void* def,const void* point){
    if(!state.capture_receiver)return state.purchase(def,point);
    std::string e;auto* r=runtime(e);if(!r){rejected(e);return false;}
    if(uintptr_t(_ReturnAddress())!=r->image+0x5c55c7){rejected("network Purchase caller differs from original UI Press");return false;}
    if(state.captured||state.pending){rejected("network intent already retained");return false;}
    action::NetworkIntent intent;uintptr_t actual_def=0,actual_region=0;region::LocalView view;
    if(!definition(*r,state.capture_receiver,intent,actual_def,e)||actual_def!=uintptr_t(def)||!r->memory.copy(uintptr_t(point),intent.position.data(),sizeof(intent.position),e)){
        rejected(e.empty()?"network Purchase actual definition differs":e);return false;}
    region::host::WorldState world;if(!region::world(*r,world,e)){rejected(e);return false;}
    if(!native_error(terrain::point_region(r->image,world.root,intent.position,actual_region),"capture region",e)||
       !region::at(*r,actual_region,0x834,intent.region,e)||!region::identity(*r,intent.region,actual_region,view,e)||!action::valid_network(intent)){
        rejected(e.empty()?"network capture scalar invalid":e);return false;}
    reader::Graph graph_before;action::NetworkState before;
    if(!graph(*r,view,graph_before,before,e)){rejected(e);return false;}
    const action::Binding binding{view.world.origin.world_epoch,view.world.origin.generation};
    const auto snapshot=net::current_snapshot_id();std::optional<action::Context> context;
    if(!capture_context(snapshot,binding,context,e)){rejected(e);return false;}
    state.captured=Pending{intent,binding,snapshot,state.token,context,before};return false;
}
void press_entry(void* receiver,void* cursor,const void* point){
    if(!state.ready||!net::session_wire::connected()){
        if(typed_action_runtime::controller_status().value("ordered",false)||(state.pending&&state.pending->context)||(state.captured&&state.captured->context)){
            rejected("Ordered Network input lost ready connected Session");return;}
        state.press(receiver,cursor,point);return;
    }
    std::string e;auto* r=runtime(e);
    if(!r||!input_admission(e)||state.pending||state.captured||state.capture_receiver){rejected(e.empty()?"network UI requires idle bound Controller":e);return;}
    uintptr_t active=0;if(!region::at(*r,uintptr_t(cursor),0x190,active,e)||active!=uintptr_t(receiver)){rejected(e.empty()?"network Press receiver is not active UI tool":e);return;}
    action::Ledger before,after;undo::UndoWitness undo_before,undo_after;std::array<float,3> position{};
    if(!region::ledger(before,e)||!undo::read_undo(r->memory,r->image,undo_before,e)||undo_before.active_graph||!r->memory.copy(uintptr_t(point),position.data(),sizeof position,e)){rejected(e.empty()?"network UI requires idle Undo":e);return;}
    state.capture_receiver=uintptr_t(receiver);const auto seh=ui_call(r->image,2,uintptr_t(receiver),uintptr_t(cursor),position);state.capture_receiver=0;
    if(!native_error(seh,"UI Press",e)){rejected(e);return;}if(!state.captured)return;
    if(!native_error(ui_call(r->image,3,0,uintptr_t(cursor),{}),"UI completion",e)||!region::ledger(after,e)||!undo::read_undo(r->memory,r->image,undo_after,e)||before!=after||undo_before!=undo_after){rejected(e.empty()?"network UI capture changed native cash/Undo":e);return;}
    uintptr_t preview=1;if(!region::at(*r,uintptr_t(receiver),0x170,preview,e)||preview){rejected(e.empty()?"network original Deactivate retained preview":e);return;}
    std::optional<action::Context> final_context;
    if(!capture_context(state.captured->snapshot,state.captured->binding,final_context,e)||final_context!=state.captured->context){
        state.capture_failed=true;rejected(e.empty()?"Network capture Context changed during original completion":e);return;}
    region::LocalView captured_view;reader::Graph captured_graph;action::NetworkState captured_state;
    if(!region::identity(*r,state.captured->intent.region,0,captured_view,e)||!graph(*r,captured_view,captured_graph,captured_state,e)||captured_state!=state.captured->graph_before){
        state.capture_failed=true;rejected(e.empty()?"Network Node capture changed complete native graph":e);return;}
    state.pending=std::move(state.captured);state.captured.reset();++state.captures;
}
}
namespace {
struct CommitStorage {alignas(8) unsigned char label[32]{},loc[56]{};bool label_ready=false,loc_ready=false;};
uint32_t commit(region::Runtime& r,uintptr_t tool,uintptr_t manager,uintptr_t def,Operation& op,const std::array<float,3>& surface,CommitStorage& storage){
    __try{
        const auto b=r.image;auto* definition=reinterpret_cast<void*>(def);const auto& point=op.request.network.position;
        ++op.calls;if(!state.purchase(definition,point.data()))return ERROR_CANCELLED;
        ++op.calls;op.created=uintptr_t(reinterpret_cast<void*(*)(void*,void*,const void*,bool)>(b+0x2a64a0)(reinterpret_cast<void*>(manager),definition,point.data(),true));
        if(!op.created)return ERROR_INVALID_DATA;
        reinterpret_cast<void(*)(void*,int32_t)>(b+0x50e810)(*reinterpret_cast<void**>(b+0x13df450),op.request.network.type=="Uplink"?0x31:0x30);
        auto allocate=reinterpret_cast<void*(*)(size_t)>(b+0xa02670);auto* graph=allocate(0x110);if(!graph)return ERROR_OUTOFMEMORY;
        reinterpret_cast<void(*)(void*)>(b+0x5400f0)(graph);
        reinterpret_cast<void(*)(void*,const char*)>(b+0x49e090)(storage.label,"{undo_msg_place}");storage.label_ready=true;
        reinterpret_cast<void(*)(void*,const void*,void*)>(b+0x538ae0)(storage.loc,storage.label,reinterpret_cast<void*>(op.created));storage.loc_ready=true;
        auto* cash=allocate(0xc8);if(!cash)return ERROR_OUTOFMEMORY;
        reinterpret_cast<void(*)(void*,int32_t,const void*)>(b+0x53ccb0)(cash,op.request.network.quoted_cost,surface.data());
        reinterpret_cast<void(*)(void*,void*)>(b+0x540760)(graph,cash);
        auto* destroy=allocate(0xc0);if(!destroy)return ERROR_OUTOFMEMORY;
        reinterpret_cast<void(*)(void*,void*)>(b+0x53dc30)(destroy,reinterpret_cast<void*>(op.created));
        reinterpret_cast<void(*)(void*,void*)>(b+0x540760)(graph,destroy);
        ++op.calls;if(!reinterpret_cast<bool(*)(void*,const void*,void*)>(b+0x589e50)(reinterpret_cast<void*>(tool),storage.loc,graph))return ERROR_INVALID_STATE;
        op.returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t cleanup(uintptr_t image,CommitStorage& storage){
    __try{if(storage.loc_ready){reinterpret_cast<void(*)(void*)>(image+0x11b2b00)(storage.loc+32);reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(storage.loc);storage.loc_ready=false;}
        if(storage.label_ready){reinterpret_cast<void(*)(void*)>(image+0x11bd1e0)(storage.label);storage.label_ready=false;}return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool inverse(region::Runtime& r,const undo::UndoWitness& before,const undo::UndoWitness& after,const Operation& op,const std::array<float,3>& position,std::string& e){
    if(after.undo.empty())return fail(e,"network Undo history is empty");undo::GraphWitness witness;const auto address=after.undo.back();
    if(!undo::read_graph(r.memory,r.image,address,witness,e)||!undo::validate_undo_commit(before,after,witness,e))return false;
    if(witness.operations!=2)return fail(e,"network Undo operation count actual="+std::to_string(witness.operations)+" required=2");
    uintptr_t data=0,cash=0,destroy=0;int32_t amount=0;uint64_t uid=0;std::array<float,3> actual{};
    if(!region::at(r,address,0x20,data,e)||!region::at(r,data,0,cash,e)||!region::at(r,data,8,destroy,e)||
       !reader::read::ap(r.memory,cash,r.image,0x1786fb0,e)||!reader::read::ap(r.memory,destroy,r.image,0x16de860,e)||
       !region::at(r,cash,0xb8,amount,e)||!region::at(r,cash,0xbc,actual,e)||!region::at(r,destroy,0xb8,uid,e))return false;
    return amount==op.request.network.quoted_cost&&actual==position&&uid==op.uid||fail(e,"network Undo actual cash/position/created UID differs");
}
}
#include "ui_network_pipe.inc"
bool enable(std::string& e){
    auto* r=runtime(e);if(!r)return false;if(state.attempted)return fail(e,"network hook installation already attempted");state.attempted=true;
    if(!code(*r,false,e))return false;
    struct Hook{uintptr_t rva;void* entry;void** original;};const Hook hooks[]{
        {0x5c5580,reinterpret_cast<void*>(&press_entry),reinterpret_cast<void**>(&state.press)},
        {0x42db30,reinterpret_cast<void*>(&purchase_entry),reinterpret_cast<void**>(&state.purchase)}};
    for(const auto& h:hooks){const auto status=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.entry,h.original);if(status!=MH_OK||!*h.original)return fail(e,"network MH_CreateHook RVA="+std::to_string(h.rva)+" status="+std::to_string(status));}
    if(!mt2mp::hooks::enable_prepared_routes(r->image,hooks,"network hook",e))return false;
    if(!code(*r,true,e)||!pipe::enable(*r,e))return false;state.ready=true;return true;
}
bool enabled()noexcept{return state.ready;}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    if(request.action==action::Action::PlaceNetworkPipe)return pipe::perform(binding,request,authority,e);
    bool ordered=false;if(!execution_admission(binding,request,authority,ordered,e))return action::Outcome::RejectedNoEffect;
    auto* r=runtime(e);if(!r||!state.ready||request.action!=action::Action::PlaceNetworkNode||!action::valid_network(request.network)){e="network runtime/intent unavailable: "+e;return action::Outcome::RejectedNoEffect;}
    if(state.operations.contains(request.action_uuid)){e="network original action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::status();
    if(!ordered&&(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid)){e="network requires exact Controller request";return action::Outcome::RejectedNoEffect;}
    region::LocalView before;reader::Graph before_graph;action::NetworkState network;action::Ledger money;undo::UndoWitness before_undo;
    uintptr_t tool=0,def=0,cursor=0,manager=0,actual_region=0;float height=0;bool legal=false;uint8_t owned=0;
    if(!code(*r,true,e)||!region::identity(*r,request.network.region,0,before,e)||!region::bound(before,binding,request.snapshot_uuid,e)||
       !graph(*r,before,before_graph,network,e)||!region::ledger(money,e)||!undo::read_undo(r->memory,r->image,before_undo,e)||!catalog(*r,request.network,tool,def,e)||
       !region::at(*r,r->image,0x13df9d0,cursor,e)||!reader::read::ap(r->memory,cursor,r->image,0x1786dd0,e)||!region::at(*r,before.region,0x850,owned,e)||
       !region::at(*r,before.region,0x978,manager,e))return action::Outcome::RejectedNoEffect;
    if(before_undo.active_graph||owned!=1||!money.record_present||money.cash<request.network.quoted_cost){e="network node requires owned region, idle Undo and sufficient actual cash";return action::Outcome::RejectedNoEffect;}
    uintptr_t shards=0,view_shard=0;
    if(!region::at(*r,r->image,0x13df440,shards,e)||!reader::read::ap(r->memory,shards,r->image,0x1702b00,e)||!region::at(*r,shards,0x60,view_shard,e))return action::Outcome::RejectedNoEffect;
    if(view_shard!=before.world.root){e="network original Purchase requires target root to be actual view shard";return action::Outcome::RejectedNoEffect;}
    if(!native_error(preflight(r->image,tool,cursor,request.network,actual_region,height,legal),"position preflight",e)||!legal||actual_region!=before.region||!std::isfinite(height)){if(e.empty())e="network original position/region preflight rejected";return action::Outcome::RejectedNoEffect;}
    if(authority&&(money!=authority->before||network!=authority->network_before)){e="network client before differs from Host";return action::Outcome::RejectedNoEffect;}
    network_history_runtime::Snapshot history_before;if(!network_history_runtime::capture(before_graph,{},history_before,e))return action::Outcome::RejectedNoEffect;
    auto& op=state.operations[request.action_uuid];op.binding=binding;op.request=request;op.ordered=ordered;op.evidence.before=money;op.evidence.network_before=network;op.attempted=true;
    auto unknown=[&]{op.cause=e;return action::Outcome::Unknown;};auto surface=request.network.position;surface[1]=height;CommitStorage storage;
    const auto called=commit(*r,tool,manager,def,op,surface,storage);const auto cleaned=cleanup(r->image,storage);
    if(called||cleaned){e="network original commit failed SEH="+std::to_string(called)+" cleanup="+std::to_string(cleaned)+" completed calls="+std::to_string(op.calls);return unknown();}
    region::LocalView after;reader::Graph after_graph;undo::UndoWitness after_undo;auto normalized=request.network;normalized.position=surface;
    if(!region::identity(*r,request.network.region,before.region,after,e)||!region::bound(after,binding,request.snapshot_uuid,e)||
       !graph(*r,after,after_graph,op.evidence.network_after,e)||!reader::delta(before_graph,after_graph,normalized,op.created,e)||
       !region::ledger(op.evidence.after,e)||!undo::read_undo(r->memory,r->image,after_undo,e))return unknown();
    const auto created_node=std::find_if(after_graph.nodes.begin(),after_graph.nodes.end(),[&](const auto& n){return n.object==op.created;});
    if(created_node==after_graph.nodes.end()){e="network created node absent from membership";return unknown();}op.uid=created_node->uid;
    if(op.calls!=3||!op.returned){e="network original Purchase/Add/Undo call count differs";return unknown();}
    if(!inverse(*r,before_undo,after_undo,op,surface,e))return unknown();op.evidence.undo_confirmed=true;
    if(!action::network_effect(request,op.evidence,e))return unknown();
    if(authority&&(op.evidence.after!=authority->after||op.evidence.network_after!=authority->network_after)){e="network original commit differs from Host result";return unknown();}
    network_history_runtime::Snapshot history_after;if(!network_history_runtime::capture(after_graph,{{op.uid,network_object_identity::birth(request.action_uuid,network_object_identity::Kind::Node)}},history_after,e)||!network_history_runtime::record(binding,request,history_before,history_after,after_undo.undo.back(),request.network.quoted_cost,e))return unknown();
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    if(request.action==action::Action::PlaceNetworkPipe)return pipe::observe(binding,request,out,e);
    auto* r=runtime(e);if(!r)return false;const auto it=state.operations.find(request.action_uuid);
    if(it==state.operations.end()||it->second.binding!=binding||it->second.request!=request||!it->second.confirmed)return fail(e,"network observation lacks exact confirmed operation");
    const auto& op=it->second;if(op.ordered&&!ordered_scope(binding,request,e))return false;region::LocalView view;reader::Graph graph_now;action::NetworkState network;action::Ledger money;
    if(!region::identity(*r,request.network.region,0,view,e)||!region::bound(view,binding,request.snapshot_uuid,e)||!graph(*r,view,graph_now,network,e)||!region::ledger(money,e))return false;
    if(network!=op.evidence.network_after||money!=op.evidence.after||std::none_of(graph_now.nodes.begin(),graph_now.nodes.end(),[&](const auto& n){return n.object==op.created&&n.uid==op.uid;}))return fail(e,"network actual state changed after original return");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){if(request.action==action::Action::PlaceNetworkPipe)return pipe::retire(binding,request,e);action::Evidence evidence;if(!observe(binding,request,evidence,e))return false;auto& op=state.operations.at(request.action_uuid);if(op.retired)return fail(e,"network operation already retired");if(!network_identity::register_place(request.action_uuid,op.created,op.uid,e)||!network_history_runtime::retire_source(binding,request,e))return false;op.retired=true;return true;}
bool admitted_node(const std::string& id,Admission& out,std::string& e){
    out={};auto* r=runtime(e);if(!r)return false;const auto it=state.operations.find(id);
    if(it==state.operations.end()||!it->second.confirmed||!it->second.retired)return fail(e,"network admission requires confirmed retired original operation");
    const auto& op=it->second;region::LocalView view;reader::Graph actual;action::NetworkState network;
    if(!region::identity(*r,op.request.network.region,0,view,e)||!region::bound(view,op.binding,op.request.snapshot_uuid,e)||!graph(*r,view,actual,network,e))return false;
    const auto found=std::find_if(actual.nodes.begin(),actual.nodes.end(),[&](const auto& n){return n.object==op.created;});
    if(found==actual.nodes.end())return fail(e,"network admission actual node membership changed");const auto& node=*found;
    if(node.object!=op.created||node.uid!=op.uid||node.region_id!=op.request.network.region||node.type!=op.request.network.type||node.key!=op.request.network.key)return fail(e,"network admission actual created node identity differs");
    out={op.binding,op.request.snapshot_uuid,op.request.canonical_record_key,node.type,node.key,node.region_id,node.uid,node.object,node.manager,node.region,node.definition};return true;
}
bool admitted_snapshot_node(const std::string& snapshot,int32_t region_id,int32_t node_index,const std::string& expected_graph,Admission& out,std::string& e){
    out={};auto* r=runtime(e);if(!r)return false;action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    region::LocalView view;reader::Graph actual;action::NetworkState network;
    if(!region::identity(*r,region_id,0,view,e)||!region::bound(view,binding,snapshot,e)||!graph(*r,view,actual,network,e))return false;
    if(expected_graph.empty()||network.graph_sha256!=expected_graph)return fail(e,"network snapshot admission full graph digest differs");
    const auto* node=network_admission::select(actual,region_id,node_index,e);if(!node)return false;
    std::string topology;if(!region::sha256(network_admission::topology(actual).dump(),topology,e))return false;
    out={binding,snapshot,{},node->type,node->key,node->region_id,node->uid,node->object,node->manager,node->region,node->definition,topology};return true;
}
bool refresh_snapshot_node(const Admission& pin,Admission& out,std::string& e){
    out={};auto* r=runtime(e);if(!r)return false;action::Binding binding;
    if(!typed_action_runtime::network_binding(pin.snapshot,binding,e))return false;
    if(binding!=pin.binding)return fail(e,"network snapshot admission Controller binding changed");
    region::LocalView view;reader::Graph actual;action::NetworkState network;
    if(!region::identity(*r,pin.region_id,pin.region,view,e)||!region::bound(view,binding,pin.snapshot,e)||!graph(*r,view,actual,network,e))return false;
    std::string topology;if(!region::sha256(network_admission::topology(actual).dump(),topology,e))return false;
    if(pin.topology_sha256.empty()||topology!=pin.topology_sha256)return fail(e,"network snapshot admission actual topology changed");
    const auto it=std::find_if(actual.nodes.begin(),actual.nodes.end(),[&](const auto& node){return node.object==pin.object;});
    if(it==actual.nodes.end()||!network_admission::same_identity(*it,pin))return fail(e,"network snapshot admission private node identity changed");
    out=pin;return true;
}
bool unresolved(){if(network_remove_runtime::unresolved())return true;if(pipe::unresolved())return true;if(state.capture_failed||state.pending||state.captured)return true;for(const auto& [id,op]:state.operations){(void)id;if(!op.retired)return true;}for(const auto& [token,job]:state.jobs){(void)token;if(!job.value("terminal",false))return true;}return false;}
void tick(){
    if(!state.ready)return;network_remove_runtime::tick();pipe::tick();const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:state.jobs){(void)token;const auto id=job.value("action_uuid",std::string{});if(id.empty()||job.value("terminal",false))continue;
        if(controller.value("action_uuid",std::string{})==id&&!controller.value("pending",true)){const auto outcome=controller.value("outcome",0);if(outcome==1||outcome==2){job["terminal"]=true;job["confirmed"]=outcome==1;job["outcome"]=outcome;job["cause"]=controller.value("cause",std::string{});}}}
    if(!state.pending)return;std::string e;bool ready=false;
    if(!capture_ready(*state.pending,ready,e)){state.capture_failed=true;rejected(e);if(!state.pending->token.empty())state.jobs[state.pending->token]["cause"]=e;return;}
    if(!ready)return;
    auto pending=*state.pending;state.pending.reset();auto* r=runtime(e);region::LocalView view;
    if(!r||!region::identity(*r,pending.intent.region,0,view,e)||!region::bound(view,pending.binding,pending.snapshot,e)){state.capture_failed=true;rejected(e);if(!pending.token.empty())state.jobs[pending.token]["cause"]=e;return;}
    const auto id=region::uuid(e);if(id.empty()||!typed_action_runtime::submit_network(id,pending.intent,e)){state.capture_failed=true;rejected(e.empty()?"network Controller submit failed":e);if(!pending.token.empty())state.jobs[pending.token]["cause"]=state.cause;return;}
    ++state.submits;state.last_action=id;if(!pending.token.empty()){state.jobs[pending.token]["action_uuid"]=id;state.jobs[pending.token]["submitted"]=true;}
}
namespace ordered_ui {json status();}
json status(){json operations=json::object();for(const auto& [id,op]:state.operations)operations[id]={{"attempted",op.attempted},{"returned",op.returned},{"confirmed",op.confirmed},{"retired",op.retired},{"native_calls",op.calls},{"ordered",op.ordered},{"cause",op.cause},{"before",op.evidence.network_before.graph_sha256},{"after",op.evidence.network_after.graph_sha256},{"ledger_before",action::management_ledger_observation(op.evidence.before)},{"ledger_after",action::management_ledger_observation(op.evidence.after)},{"undo_confirmed",op.evidence.undo_confirmed},{"local_uid",op.uid}};
    return {{"ordered_inputs",ordered_ui::status()},{"remove_capture",network_remove_runtime::capture_status()},{"remove",network_remove_runtime::status()},{"history",network_history_runtime::status()},{"pipe",pipe::status()},{"enabled",state.ready},{"capture_count",state.captures},{"submitted",state.submits},{"rejected",state.rejections},{"pending",bool(state.pending)||bool(state.captured)},{"last_action",state.last_action},{"cause",state.cause},{"operations",operations},{"ui_jobs",state.jobs}};}
bool query(const json& args,json& out,std::string& e){
    if(!args.is_object()||args.size()!=1||!args.contains("id")||!args.at("id").is_number_integer())return fail(e,"read network placement requires exact region id");
    const auto id=args.at("id").get<int64_t>();if(id<0||id>INT32_MAX)return fail(e,"network region ID outside int32");auto* r=runtime(e);if(!r)return false;
    region::LocalView view;reader::Graph actual;action::NetworkState network;if(!region::identity(*r,int32_t(id),0,view,e)||!graph(*r,view,actual,network,e))return false;
    json identities=json::array();
    auto identity=[&](uintptr_t object,uint64_t uid,const json& index,const char* kind){std::string key;network_identity::ObjectEntry entry;
        if(!network_identity::object_key(object,uid,key,entry,e))return false;
        identities.push_back({{"record_key",key},{"kind",kind},{"region_id",index[0]},{"index",index[1]},{"local_uid",uid}});return true;};
    for(const auto& node:actual.nodes)if(!identity(node.object,node.uid,actual.node_keys.at(node.object),"node"))return false;
    for(const auto& pipe:actual.pipe_values)if(!identity(pipe.object,pipe.uid,actual.pipe_keys.at(pipe.object),"pipe"))return false;
    out={{"nodes",network.nodes},{"pipes",network.pipes},{"connections",network.connections},{"graph_sha256",network.graph_sha256},{"rows",actual.rows},{"identities",identities},{"ui_network",status()}};return true;
}
bool execute_native_place(const json& args,json& out,std::string& e){
    out=json::object();auto* r=runtime(e);if(!r)return false;
    if(!state.ready||unresolved()||!net::session_wire::connected())return fail(e,"network UI requires ready idle connected Controller");
    if(!input_admission(e))return false;
    if(!args.is_object()||args.size()!=5||!args.contains("token")||!args.contains("id")||!args.contains("type")||!args.contains("key")||!args.contains("position")||
       !args.at("token").is_string()||!args.at("id").is_number_integer()||!args.at("type").is_string()||!args.at("key").is_string())return fail(e,"network native UI requires token, id, type, key, position");
    const auto token=args.at("token").get<std::string>();const auto id=args.at("id").get<int64_t>();action::NetworkIntent intent;
    if(token.empty()||token.size()>128||token.find('\0')!=std::string::npos||state.jobs.contains(token)||id<0||id>INT32_MAX)return fail(e,"network UI token/id invalid or already attempted");
    intent.region=int32_t(id);intent.type=args.at("type").get<std::string>();intent.key=args.at("key").get<std::string>();const auto& position=args.at("position");
    if(!position.is_array()||position.size()!=3)return fail(e,"network position requires three coordinates");
    for(size_t i=0;i<3;++i){if(!position[i].is_number())return fail(e,"network coordinate not numeric");const auto value=position[i].get<double>();if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return fail(e,"network coordinate outside finite float32");intent.position[i]=float(value);}
    if(!action::valid_network(intent)||!code(*r,true,e))return false;region::LocalView view;if(!region::identity(*r,intent.region,0,view,e))return false;
    placement_ui_probe::Context context{r->image,uintptr_t(view.world.origin.state),view.world.origin.world_epoch,view.world.origin.generation,r->thread};
    placement_ui_probe::Request request;request.token=token;request.family=placement_ui_probe::Family::NetworkNode;request.type=intent.type;request.variant=intent.key;
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding binding;if(!placement_ui_probe::read_existing_tool(memory,context,request,binding,e))return false;
    uintptr_t def=0,actual_region=0;float height=0;bool legal=false;
    if(!definition(*r,binding.behaviour,intent,def,e)||!native_error(preflight(r->image,binding.behaviour,binding.cursor,intent,actual_region,height,legal),"UI preflight",e)||!legal||actual_region!=view.region)return fail(e,e.empty()?"network original UI position rejected":e);
    auto& job=state.jobs[token];job={{"token",token},{"attempted",0},{"returned",0},{"captured",false},{"submitted",false},{"confirmed",false},{"terminal",false},{"action_uuid",""},{"cause",""}};
    state.token=token;struct Reset{~Reset(){state.token.clear();}} reset;const auto captures=state.captures;
    auto call=[&](unsigned step,uintptr_t object){job["attempted"]=job.at("attempted").get<unsigned>()+1;const auto seh=ui_call(r->image,step,object,binding.cursor,intent.position);if(seh)return native_error(seh,"UI input",e);job["returned"]=job.at("returned").get<unsigned>()+1;return true;};
    bool ok=call(0,binding.button);uintptr_t active=0;if(ok)ok=region::at(*r,binding.cursor,0x190,active,e)&&active==binding.behaviour;
    if(ok)ok=call(1,binding.behaviour)&&call(4,binding.behaviour);
    if(ok)ok=state.captures==captures+1&&state.pending&&state.pending->token==token;
    if(!ok){if(e.empty())e="network genuine UI returned without one captured intent: "+state.cause;job["cause"]=e;out=job;return false;}
    job["captured"]=true;job["type"]=intent.type;job["key"]=intent.key;job["position"]=intent.position;job["cost"]=intent.quoted_cost;out=job;return true;
}
namespace ordered_ui {
struct Input {json args,native=json::object();action::Context context;action::Binding binding;bool pipe=false,remove=false,attempted=false,returned=false;std::string cause;};
std::map<std::string,Input> jobs;std::optional<std::string> pending;
bool current(const Input& job,std::string& e){
    action::Context context;action::Binding binding;
    if(!action::selected_session().current(context,e)||!typed_action_runtime::read_binding(job.context.snapshot_uuid,binding,e))return false;
    return (context==job.context&&binding==job.binding&&net::current_snapshot_id()==job.context.snapshot_uuid&&
        Config::instance().is_host==(context.role==action::Role::Host))||fail(e,"Ordered Network input Context/Binding/role changed");
}
json job_json(const std::string& token,const Input& job){return {{"token",token},{"pipe",job.pipe},{"remove",job.remove},{"ordered",true},
    {"queued",!job.attempted&&job.cause.empty()},{"input_attempted",job.attempted},{"input_returned",job.returned},
    {"session",job.context.session},{"snapshot_uuid",job.context.snapshot_uuid},{"peer_mask",job.context.peer_mask},
    {"local_peer",job.context.local_peer},{"host_peer",job.context.host_peer},{"native",job.native},{"cause",job.cause}};}
json status(){json out=json::object();for(const auto& [token,job]:jobs)out[token]=job_json(token,job);return out;}
bool enqueue(const json& args,bool pipe,json& out,std::string& e,bool remove=false){
    const auto controller=typed_action_runtime::controller_status();bool inside=false;
    if(!Config::instance().local_test||!controller.value("ordered",false)||!controller.value("started",false)||controller.value("locked",true)||
       pending||unresolved()||jobs.size()>=64||!typed_action_runtime::original_go_admission(inside,e))
        return fail(e,e.empty()?"Ordered Network enqueue requires ready owned Controller":e);
    if(controller.value("pending",true)&&!inside)return fail(e,"Ordered Network enqueue outside OriginalGo requires idle Controller");
    const char* token_key=remove?"diagnostic_uuid":"token";
    if(!args.is_object()||!args.contains(token_key)||!args.at(token_key).is_string())return fail(e,"Ordered Network input requires native token");
    const auto token=args.at(token_key).get<std::string>();
    if(token.empty()||token.size()>128||token.find('\0')!=std::string::npos||jobs.contains(token))return fail(e,"Ordered Network input token invalid or already reserved - no replay");
    for(const auto& [id,prior]:jobs){(void)id;if(!prior.returned)return fail(e,"Prior ordered Network input failed or uncertain - no new input");}
    Input job;job.args=args;job.pipe=pipe;job.remove=remove;
    if(!action::selected_session().current(job.context,e)||!typed_action_runtime::read_binding(job.context.snapshot_uuid,job.binding,e)||!current(job,e))return false;
    jobs.emplace(token,std::move(job));pending=token;out=job_json(token,jobs.at(token));return true;
}
}
void tick_original_input(){
    namespace u=ordered_ui;if(!u::pending)return;bool inside=false;std::string e;
    if(!typed_action_runtime::original_go_admission(inside,e)||!inside){if(e.empty())return;}
    const auto token=*u::pending;auto& job=u::jobs.at(token);u::pending.reset();
    if(e.empty()&&!u::current(job,e)&&e.empty())e="Ordered Network queued owner retired";
    try{if(e.empty()){job.attempted=true;job.returned=job.remove?network_remove_runtime::original_ui(job.args,job.native,e):job.pipe?pipe::native_ui(job.args,job.native,e):execute_native_place(job.args,job.native,e);}}
    catch(const std::exception& ex){e=std::string("Ordered Network original input exception: ")+ex.what();}
    catch(...){e="Ordered Network original input non-standard exception";}
    job.cause=e;if(!job.returned){if(job.cause.empty())job.cause="Ordered Network original input unconfirmed";LOG_ERROR("[NetworkUI] token={} cause={}; no replay",token,job.cause);}
}
bool native_pipe(const json& args,json& out,std::string& e){
    return typed_action_runtime::controller_status().value("ordered",false)?ordered_ui::enqueue(args,true,out,e):pipe::native_ui(args,out,e);
}
bool native_place(const json& args,json& out,std::string& e){
    return typed_action_runtime::controller_status().value("ordered",false)?ordered_ui::enqueue(args,false,out,e):execute_native_place(args,out,e);
}

}

#include "network_remove_runtime.inc"
#include "network_remove_ui.inc"
#include "network_history_runtime.inc"
