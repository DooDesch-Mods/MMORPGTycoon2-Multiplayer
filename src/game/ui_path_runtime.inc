// Uses the existing Region identity and Terrain side-effect readers in this TU.
#include "ui_path_runtime.h"
#include "path_reader.h"
#include "path_codeguards.h"
#include "path_history_graph.h"
#include "mixed_path_state.h"
#include "mixed_path_identity.h"
#include "wall_pass_cursor.h"
#include "wall_private_lifecycle.h"
#include "wall_private_codes.h"
#include "building_history_slice/cash_effect.h"
#include "move_contract.h"
namespace mt2mp::game::typed_action_runtime {bool submit_path_delete(const std::string&,const action_session::PathDeleteIntent&,std::string&);bool submit_path(const std::string&,const action_session::PathIntent&,std::string&);bool submit_world_path(const std::string&,const action_session::WorldPathIntent&,std::string&);}
namespace mt2mp::game::ui_path_runtime {
namespace world_path_runtime {
bool capturing_wall();
bool executing_wall();
bool pending();
bool wall_purchase(ui_region_runtime::Runtime&,int32_t,float,const void*,uintptr_t,std::string&);
bool wall_replay_purchase(ui_region_runtime::Runtime&,int32_t,float,const void*,std::string&);
void rejected(const std::string&);
}
namespace {
namespace action=action_session;
namespace region=ui_region_runtime;
namespace terrain=ui_terrain_runtime;
namespace native=path_native;
namespace undo=build_actions::private_native;
using json=nlohmann::json;
using Press=void(*)(void*,void*,const void*);
using Purchase=bool(*)(int32_t,float,const void*);
using Unary=void(*)(void*);
using RemoveUI=bool(*)(void*,void*);
struct PendingDelete {action::PathDeleteIntent intent;action::Binding binding;std::string snapshot,token;};
struct Graph {action::PathState state;std::vector<uintptr_t> paths,nodes;std::vector<uint64_t> path_uids,node_uids;json rows;};
struct Pending {native::Preview preview;action::Binding binding;std::string snapshot,token;};
struct Operation {action::Binding binding;action::Request request;action::Evidence evidence;uintptr_t receiver=0;bool attempted=false,confirmed=false,retired=false,constructed=false;unsigned purchases=0;uint32_t exception=0;std::string cause;std::vector<uint64_t> uids;std::optional<path_history_graph::Snapshot> history_before,history_after;uintptr_t history_graph=0;};
struct Runtime {
    bool attempted=false,ready=false;Press press=nullptr;Purchase purchase=nullptr;RemoveUI remove_ui=nullptr;
    uintptr_t ui_receiver=0;Operation* replay=nullptr;
    std::optional<Pending> pending,captured;std::optional<PendingDelete> delete_pending;
    std::map<std::string,Operation> operations;std::map<std::string,json> jobs;
    uint64_t capture_count=0,submitted=0,rejected=0;int32_t road=-1,node=-1;
    std::string cause,last_action,token;
};
Runtime path;
bool fail(std::string& e,const std::string& why){e=why;return false;}
region::Runtime* runtime(std::string& e){return terrain::runtime(e);}
void rejected(const std::string& e){++path.rejected;if(path.cause!=e){path.cause=e;LOG_ERROR("[UIPath] Rejected cause={}",e);}}
bool native_error(uint32_t seh,const char* operation,std::string& e){return !seh||fail(e,std::string("path original ")+operation+" SEH="+std::to_string(seh));}
uint32_t enums(uintptr_t image,int32_t& road,int32_t& node) {
    const region::NativeString a{"Road",4,{}},b{"RoadNode",8,{}};
    __try{road=reinterpret_cast<int32_t(*)(const void*)>(image+0x4393b0)(&a);node=reinterpret_cast<int32_t(*)(const void*)>(image+0x4214a0)(&b);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool graph(region::Runtime& r,const region::LocalView& view,Graph& out,std::string& e) {
    int32_t road=path.road,node=path.node;
    if(road<0||node<0){const auto seh=enums(r.image,road,node);
        if(seh||road<0||node<0)return fail(e,"path Region census enum lookup failed SEH="+std::to_string(seh));}
    uintptr_t manager=0,owner=0;if(!region::at(r,view.region,0x9a0,manager,e)||!region::at(r,manager,0x6a0,owner,e))return false;
    if(owner!=view.region)return fail(e,"path manager region owner differs");
    if(!native::array(r.memory,manager,0x28+size_t(road)*0x30,0x30+size_t(road)*0x30,0x34+size_t(road)*0x30,1000000,out.paths,e)||
       !native::array(r.memory,manager,0x238+size_t(node)*0x30,0x240+size_t(node)*0x30,0x244+size_t(node)*0x30,1000000,out.nodes,e))return false;
    std::map<uintptr_t,size_t> paths,nodes;
    for(size_t i=0;i<out.paths.size();++i)if(!out.paths[i]||!paths.emplace(out.paths[i],i).second)return fail(e,"path manager contains null/duplicate road");
    for(size_t i=0;i<out.nodes.size();++i)if(!out.nodes[i]||!nodes.emplace(out.nodes[i],i).second)return fail(e,"path manager contains null/duplicate node");
    json rows=json::array(),node_rows=json::array();
    for(auto p:out.paths) {
        uintptr_t start=0,end=0,owner_region=0;int32_t type=-1;uint64_t uid=0;uint8_t preview=0;
        std::vector<action::PathPoint> points,walks;std::vector<float> distances,walk_distances;
        if(!native::ap(r.memory,p,r.image,0x1743f10,e)||!region::at(r,p,0x18,uid,e)||!region::at(r,p,0x21c,type,e)||!region::at(r,p,0x178,owner_region,e)||
           !region::at(r,p,0x268,start,e)||!region::at(r,p,0x270,end,e)||!region::at(r,p,0x46e,preview,e)||
           !native::array(r.memory,p,0x298,0x2a0,0x2a4,65536,points,e)||!native::array(r.memory,p,0x2b0,0x2b8,0x2bc,65536,distances,e))return false;
        // Only Bridge uses separate walkpoints/distances; Road's reserved distance storage is not initialized.
        if(type==6&&(!native::array(r.memory,p,0x2c8,0x2d0,0x2d4,65536,walks,e)||
           !native::array(r.memory,p,0x2e0,0x2e8,0x2ec,65536,walk_distances,e)))return false;
        if(type!=road)return native::mismatch(e,"road type",uint32_t(type),uint32_t(road));
        if(owner_region!=view.region)return native::mismatch(e,"road owner",owner_region,view.region);
        if(preview)return native::mismatch(e,"road preview",preview,0);
        if(!uid)return fail(e,"path road UID actual=0 required=nonzero");
        if(!nodes.contains(start))return fail(e,"path road start actual="+std::to_string(start)+" required=member of RoadNode bucket");
        if(!nodes.contains(end))return fail(e,"path road end actual="+std::to_string(end)+" required=member of RoadNode bucket");
        if(start==end)return fail(e,"path road endpoints actual="+std::to_string(start)+" required=distinct");
        if(points.size()<2)return fail(e,"path road waypoint count actual="+std::to_string(points.size())+" required>=2");
        // GetWalkpointCount uses the separate array only for native type6.
        const size_t walk_count=type==6?walks.size():points.size();
        if(walk_count<2)return fail(e,"path road walkpoint count actual="+std::to_string(walk_count)+" required>=2 type="+std::to_string(type));
        for(const auto* list:{&points,&walks})for(const auto& point:*list)for(float value:point)if(!std::isfinite(value))return fail(e,"path native waypoint is not finite");
        for(const auto* list:{&distances,&walk_distances})for(float value:*list)if(!std::isfinite(value))return fail(e,"path native distance is not finite");
        rows.push_back({nodes.at(start),nodes.at(end),points,distances,walks,walk_distances});out.path_uids.push_back(uid);
    }
    for(auto n:out.nodes) {
        int32_t type=-1;uint64_t uid=0;action::PathPoint position{};std::vector<uintptr_t> connected;
        if(!native::ap(r.memory,n,r.image,0x1700a10,e)||!region::at(r,n,0x18,uid,e)||!region::at(r,n,0x298,type,e)||!region::at(r,n,0x110,position,e)||
           !native::array(r.memory,n,0x2c8,0x2d0,0x2d4,65536,connected,e))return false;
        if(type!=node||!uid)return fail(e,"path actual node type/UID differs");
        for(float value:position)if(!std::isfinite(value))return fail(e,"path node position is not finite");
        json edges=json::array();for(auto p:connected){if(!paths.contains(p))return fail(e,"path node refers outside current road topology");edges.push_back(paths.at(p));}
        node_rows.push_back({position,edges});out.node_uids.push_back(uid);
    }
    out.rows={{"paths",rows},{"nodes",node_rows}};out.state.paths=out.paths.size();out.state.nodes=out.nodes.size();
    return region::sha256(out.rows.dump(),out.state.graph_sha256,e);
}
bool endpoint_binding(const Graph& graph,const action::PathIntent& intent,std::string& e) {
    if(!action::valid_path(intent))return fail(e,"path endpoint intent invalid");
    if(action::path_connected(intent)&&graph.state.graph_sha256!=intent.graph_before_sha256)return fail(e,"path endpoint graph changed actual="+graph.state.graph_sha256+" required="+intent.graph_before_sha256);
    for(const auto& endpoint:{std::pair{intent.start_node,intent.start},std::pair{intent.end_node,intent.end}}){
        if(endpoint.first<0)continue;
        if(size_t(endpoint.first)>=graph.nodes.size())return fail(e,"path endpoint index actual="+std::to_string(endpoint.first)+" required<count "+std::to_string(graph.nodes.size()));
        if(graph.rows.at("nodes")[size_t(endpoint.first)][0]!=json(endpoint.second))return fail(e,"path endpoint position differs from pinned graph node index="+std::to_string(endpoint.first));
    }return true;
}
bool bind_preview(const Graph& graph,native::Preview& preview,std::string& e) {
    auto index=[&](uintptr_t pointer,int32_t& out){out=-1;if(!pointer)return true;
        const auto found=std::find(graph.nodes.begin(),graph.nodes.end(),pointer);
        if(found==graph.nodes.end())return fail(e,"path selected endpoint not a member of current RoadNode graph");
        out=int32_t(found-graph.nodes.begin());return true;};
    if(!index(preview.start_node,preview.intent.start_node)||!index(preview.end_node,preview.intent.end_node))return false;
    preview.intent.graph_before_sha256=action::path_connected(preview.intent)?graph.state.graph_sha256:std::string{};
    return endpoint_binding(graph,preview.intent,e);
}
uint32_t native_call(uintptr_t image,unsigned step,uintptr_t receiver,uintptr_t cursor,const action::PathPoint& point) {
    __try {
        auto* p=reinterpret_cast<void*>(receiver);
        if(step==0)reinterpret_cast<Unary>(image+0x580870)(p);
        else if(step==1)reinterpret_cast<Press>(image+0x5cb910)(p,reinterpret_cast<void*>(cursor),point.data());
        else if(step==2)path.press(p,reinterpret_cast<void*>(cursor),point.data());
        else if(step==3){reinterpret_cast<Unary>(image+0x5ca660)(p);reinterpret_cast<Unary>(image+0x5ca570)(p);}
        else if(step==4)reinterpret_cast<Unary>(image+0x5ca000)(p);
        else if(step==5)reinterpret_cast<Press>(image+0x5cc2b0)(p,reinterpret_cast<void*>(cursor),point.data());
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t construct(uintptr_t image,Operation& operation) {
    const region::NativeString road{"Road",4,{}},node{"RoadNode",8,{}};
    __try {
        operation.receiver=uintptr_t(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x238));
        auto* p=reinterpret_cast<unsigned char*>(operation.receiver);if(!p)return ERROR_OUTOFMEMORY;
        reinterpret_cast<Unary>(image+0x5cca40)(p);operation.constructed=true;
        reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(p+0x148,&road);
        reinterpret_cast<void(*)(void*,const void*)>(image+0x11bfce0)(p+0x168,&node);
        *reinterpret_cast<int32_t*>(p+0x188)=path.road;*reinterpret_cast<int32_t*>(p+0x18c)=path.node;
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool remove_ui_entry(void*,void*);
void press_entry(void*,void*,const void*);
bool purchase_entry(int32_t type,float length,const void* point);
bool code(region::Runtime& r,bool hooked,std::string& e) {
    const std::array<uintptr_t,3> originals{uintptr_t(path.press),uintptr_t(path.purchase),uintptr_t(path.remove_ui)};
    const std::array<uintptr_t,3> entries{uintptr_t(&press_entry),uintptr_t(&purchase_entry),uintptr_t(&remove_ui_entry)};
    for(size_t i=0;i<std::size(path_codes);++i){const auto& g=path_codes[i];std::vector<uint8_t> bytes(g.size);
        if(!r.memory.copy(r.image+g.rva,bytes.data(),bytes.size(),e))return false;
        if(!hooked||i>=3){if(std::memcmp(bytes.data(),g.bytes,g.size))return fail(e,"path exact code differs RVA="+std::to_string(g.rva));continue;}
        if(bytes[0]!=0xe9||std::memcmp(bytes.data()+5,g.bytes+5,g.size-5))return fail(e,"path owned hook tail differs");
        std::vector<uint8_t> prefix(g.span);if(!r.memory.copy(originals[i],prefix.data(),prefix.size(),e)||std::memcmp(prefix.data(),g.bytes,g.span))return fail(e,"path original trampoline differs");
        constexpr std::array<uint8_t,6> jump{0xff,0x25,0,0,0,0};std::array<uint8_t,6> actual{};uintptr_t target=0;
        if(!r.memory.copy(originals[i]+g.span,actual.data(),6,e)||actual!=jump||!r.memory.copy(originals[i]+g.span+6,&target,8,e)||target!=r.image+g.rva+g.span)return fail(e,"path trampoline continuation differs");
        int32_t d=0;std::memcpy(&d,bytes.data()+1,4);const auto relay=uintptr_t(int64_t(r.image+g.rva+5)+d);
        if(!r.memory.copy(relay,actual.data(),6,e)||actual!=jump||!r.memory.copy(relay+6,&target,8,e)||target!=entries[i])return fail(e,"path hook relay owner differs");
    }return true;
}
bool capture(region::Runtime& r,int32_t type,float length,const void* position,std::string& e) {
    if(path.captured||path.pending)return fail(e,"path intent already retained");
    native::Preview preview;if(!native::preview(r.memory,r.image,path.ui_receiver,path.road,path.node,preview,e))return false;
    action::PathPoint point{};if(!r.memory.copy(uintptr_t(position),point.data(),sizeof(point),e))return false;
    if(type!=preview.type||length!=preview.intent.quoted_length||point!=preview.intent.end)return fail(e,"path actual Purchase parameters differ from preview");
    region::LocalView view;if(!region::identity(r,preview.intent.region,preview.region,view,e))return false;
    Graph actual;if(!graph(r,view,actual,e)||!bind_preview(actual,preview,e))return false;
    path.captured=Pending{preview,{view.world.origin.world_epoch,view.world.origin.generation},net::current_snapshot_id(),path.token};return true;
}
bool purchase_entry(int32_t type,float length,const void* point) {
    std::string e;auto* r=runtime(e);const auto caller=uintptr_t(_ReturnAddress());
    if(r&&world_path_runtime::capturing_wall()){
        if(!world_path_runtime::wall_purchase(*r,type,length,point,caller,e))world_path_runtime::rejected(e);
        return false;
    }
    if(world_path_runtime::executing_wall()){
        if(!r){world_path_runtime::rejected(e.empty()?"Wall private Purchase lacks Region runtime":e);return false;}
        const bool paid=world_path_runtime::wall_replay_purchase(*r,type,length,point,e);
        if(!paid)world_path_runtime::rejected(e.empty()?"Wall private original Purchase returned false":e);
        return paid;
    }
    if(r&&path.ready&&path.ui_receiver&&!path.replay){
        try{if(caller!=r->image+0x5cc593&&caller!=r->image+0x5cc498)rejected("path Purchase caller differs from native completion routes");else if(!capture(*r,type,length,point,e))rejected(e);}catch(const std::exception& x){rejected(std::string("path Purchase capture exception: ")+x.what());}catch(...){rejected("path Purchase capture nonstandard exception");}
        return false;
    }
    if(r&&path.replay){
        if(caller!=r->image+0x5cc593&&caller!=r->image+0x5cc498){path.replay->cause="path private Purchase caller differs";return false;}
        ++path.replay->purchases;
    }
    return path.purchase(type,length,point);
}
void press_entry(void* receiver,void* cursor,const void* point) {
    if(!path.ready||!net::session_wire::connected()||path.replay){path.press(receiver,cursor,point);return;}
    std::string e;auto* r=runtime(e);const auto controller=typed_action_runtime::status();
    if(!r||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||path.pending||path.captured||path.delete_pending||path.ui_receiver||world_path_runtime::pending()){rejected(e.empty()?"path UI requires idle bound Controller":e);return;}
    action::Ledger before,after;undo::UndoWitness undo_before,undo_after;
    if(!region::ledger(before,e)||!undo::read_undo(r->memory,r->image,undo_before,e)||undo_before.active_graph){rejected(e.empty()?"path UI requires idle Undo":e);return;}
    action::PathPoint actual_point{};if(!r->memory.copy(uintptr_t(point),actual_point.data(),sizeof(actual_point),e)){rejected(e);return;}
    path.ui_receiver=uintptr_t(receiver);
    const auto seh=native_call(r->image,2,uintptr_t(receiver),uintptr_t(cursor),actual_point);path.ui_receiver=0;
    if(!native_error(seh,"UI Press",e)){rejected(e);return;}
    if(!path.captured)return;
    auto held=*path.captured;
    if(!native_error(native_call(r->image,3,uintptr_t(receiver),uintptr_t(cursor),{}),"captured preview Deactivate/Activate",e)||
       !region::ledger(after,e)||!undo::read_undo(r->memory,r->image,undo_after,e)||before!=after||undo_before!=undo_after){rejected(e.empty()?"path capture changed cash/Undo":e);return;}
    uintptr_t ghost=0;uint8_t active=1;if(!region::at(*r,uintptr_t(receiver),0x1a8,ghost,e)||!region::at(*r,uintptr_t(receiver),0x230,active,e)||ghost||active){rejected(e.empty()?"path original cleanup retained preview":e);return;}
    path.pending=std::move(held);path.captured.reset();++path.capture_count;(void)point;
}
}
#include "world_path_sample.inc"
#include "world_path_capture.inc"
#include "world_path_wall_native.inc"
#include "world_path_replay.inc"
#include "path_delete_execution.inc"
#include "path_execution.inc"
#include "world_path_history.inc"
#include "path_history_runtime.inc"
void retire_world(){world_path_runtime::retire_world();}
}
