// Included after ui_region_runtime.cpp's namespace. Region identity and complete
// scenery/ledger readback are shared locally; wire types contain no native owner.
#include "ui_terrain_runtime.h"
#include "scenery_make_history.h"
#include "group_reader.h"
#include "duplicate_calls.h"
#include "native_polyshape.h"
#include "terrain_codeguards.h"
#include "terrain_height_grid.h"
#include "terrain_height_codeguards.h"
#include "terrain_height_ui_codeguards.h"
#include "placement_ui_probe/ui_probe.h"
#include "placement_ui_diagnostics/placement_ui_diagnostics.h"
#include "object_identity/remove_runtime/copy/undo_witness.h"
namespace mt2mp::game::typed_action_runtime {
bool submit_terrain(const std::string&,const action_session::TerrainIntent&,std::string&);
}
namespace mt2mp::game::ui_terrain_runtime {
namespace {
namespace region=ui_region_runtime;
namespace action=action_session;
namespace undo=build_actions::private_native;
using json=nlohmann::json;
using Unary=void(*)(void*);
using Binary=void(*)(void*,const void*);
using Press=void(*)(void*,void*,const void*);
using Cancel=void(*)(void*,void*);
struct Stroke {
    uintptr_t brush=0,graph=0;
    bool cleanup_attempted=false;
    action::TerrainIntent intent;
    action::Binding binding;
    std::string snapshot,cause;
    std::string ui_token;
    undo::UndoWitness before_undo;
};
struct NativeBrush {alignas(16) unsigned char bytes[0x680]{};};
struct TerrainOperation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    std::unique_ptr<NativeBrush> brush=std::make_unique<NativeBrush>();
    bool constructed=false,attempted=false,returned=false,confirmed=false,retired=false;
    uintptr_t graph=0;int32_t native_cost=-1;float changed_area=0;
    std::vector<uint64_t> removed_scenery_uids;std::vector<std::string> keys;
    std::vector<uint64_t> history_before_uids,history_after_uids;
    std::map<uint64_t,scenery_make_history::Saved> history_scenery;
    std::map<uint64_t,std::string> history_keys;
    std::string cause;
};
struct TerrainRuntime {
    bool ready=false,attempted=false;
    Press press=nullptr;Binary start=nullptr,paint=nullptr,finished=nullptr;
    Unary end=nullptr;Cancel cancel=nullptr;
    uintptr_t expected=0;
    undo::UndoWitness expected_undo;
    std::optional<Stroke> capture,pending;
    std::map<std::string,TerrainOperation> operations;
    uint64_t captured=0,submitted=0,rejected=0;
    std::string cause,last_action;
    std::string ui_token;
    std::map<std::string,json> ui_jobs;
};
TerrainRuntime terrain;
bool fail(std::string& e,const std::string& why){e=why;return false;}
region::Runtime* runtime(std::string& e) {
    auto* r=region::published.load(std::memory_order_acquire);
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread){e="terrain requires enabled Region readback on Mainthread";return nullptr;}return r;
}
uint32_t unary(Unary fn,void* receiver){__try{fn(receiver);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t binary(Binary fn,void* receiver,const void* parameter){__try{fn(receiver,parameter);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t shape_get(uintptr_t image,uintptr_t region,int32_t style,void* out,bool& constructed) {
    __try{reinterpret_cast<void*(*)(void*,void*,int32_t)>(image+0x362450)(out,reinterpret_cast<void*>(region+0x78),style);constructed=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t shape_make(uintptr_t image,poly::Storage& out,const poly::VectorView& paths,bool& constructed) {
    __try {
        reinterpret_cast<Unary>(image+0xf1810)(&out);constructed=true;
        auto* data=*reinterpret_cast<void**>(out.bytes.data()+0x18);
        reinterpret_cast<Binary>(image+0xf7ed0)(data,&paths);
        reinterpret_cast<Unary>(image+0xf3b60)(&out);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
json shape_json(const action::TerrainShape& shape) {
    json paths=json::array();for(const auto& path:shape){json p=json::array();for(const auto& v:path){p.push_back(v.x);p.push_back(v.z);}paths.push_back(std::move(p));}return paths;
}
bool shapes(region::Runtime& r,uintptr_t receiver,std::array<action::TerrainShape,4>& out,std::string& e) {
    for(int32_t i=0;i<4;++i) {
        poly::Storage shape;bool constructed=false;const auto seh=shape_get(r.image,receiver,i,&shape,constructed);
        bool read=false;if(!seh)read=poly::read(r.memory,uintptr_t(&shape),out[size_t(i)],e);
        const auto cleanup=constructed?unary(reinterpret_cast<Unary>(r.image+0xf15a0),&shape):0;
        if(seh||cleanup)return fail(e,"terrain original GetShapeOfStyle SEH="+std::to_string(seh)+" cleanup="+std::to_string(cleanup));
        if(!read)return false;
    }return true;
}
bool contour_hash(region::Runtime& r,uintptr_t receiver,std::string& out,std::string& e) {
    std::array<action::TerrainShape,4> contours;if(!shapes(r,receiver,contours,e))return false;
    json value=json::array();for(const auto& contour:contours)value.push_back(shape_json(contour));
    return region::sha256(value.dump(),out,e);
}
bool bug_rows(region::Runtime& r,uintptr_t receiver,action::TerrainState& out,std::string& e,json* canonical=nullptr) {
    json rows=json::array();uint64_t total=0;
    for(size_t family=0;family<2;++family) {
        uintptr_t manager=0;if(!region::at(r,receiver,family?0x998:0x950,manager,e))return false;
        for(size_t bucket=0;bucket<(family?3u:1u);++bucket) {
            const size_t offset=family?0x40+bucket*0x30:0x410;
            uintptr_t data=0;int32_t count=0,capacity=0;
            if(!region::at(r,manager,offset,data,e)||!region::at(r,manager,offset+8,count,e)||!region::at(r,manager,offset+12,capacity,e))return false;
            if(count<0||count>1000000||capacity<count||(!data&&count))return fail(e,"terrain bug-target native array differs");
            for(int32_t i=0;i<count;++i) {
                uintptr_t target=0,ap=0,collection=0;std::array<float,3> position{};
                if(!region::at(r,data,size_t(i)*8,target,e)||!region::at(r,target,0,ap,e)||!region::at(r,target,0x110,position,e)||!region::at(r,target,family?0x848:0xbd8,collection,e))return false;
                if(ap<r.image||ap-r.image>0x2000000)return fail(e,"terrain bug target native class outside exact image");
                for(float f:position)if(!std::isfinite(f))return fail(e,"terrain bug-target position is not finite");
                json bugs=json::array();
                if(collection) {
                    uintptr_t collection_ap=0,anchor=0;
                    if(!region::at(r,collection,0,collection_ap,e)||!region::at(r,collection,0x48,anchor,e))return false;
                    if(collection_ap!=r.image+0x1705640)return fail(e,"terrain bug collection class differs");
                    if(anchor!=target)return fail(e,"terrain bug collection anchor differs from containing prop");
                    uintptr_t objects=0;int32_t size=0,cap=0;
                    if(!region::at(r,collection,0x20,objects,e)||!region::at(r,collection,0x28,size,e)||!region::at(r,collection,0x2c,cap,e))return false;
                    if(size<0||size>1000000||cap<size||(!objects&&size)||total+uint64_t(size)>1000000)return fail(e,"terrain bug collection span differs");
                    for(int32_t b=0;b<size;++b) {
                        uintptr_t bug=0,bug_ap=0,bug_anchor=0;std::array<uint8_t,27> values{};
                        if(!region::at(r,objects,size_t(b)*8,bug,e)||!region::at(r,bug,0,bug_ap,e)||!region::at(r,bug,0x50,bug_anchor,e)||!region::at(r,bug,0x2c,values,e))return false;
                        if(bug_ap!=r.image+0x1742200)return fail(e,"terrain bug class differs");
                        if(bug_anchor!=target)return fail(e,"terrain bug anchor differs from containing prop");
                        // Six initialized 32-bit values and three flags. The local
                        // Bug ID at +0x28 and pointers at +0x48/+0x50 stay local.
                        bugs.push_back(values);
                    }total+=uint64_t(size);
                }
                // Array order is the original SpawnBugs target order. Keep it in
                // the digest, even when two props share a position and class.
                rows.push_back({family,bucket,i,ap-r.image,position,std::move(bugs)});
            }
        }
    }
    out.bug_count=total;if(!region::sha256(rows.dump(),out.bugs_sha256,e))return false;
    if(canonical)*canonical=std::move(rows);return true;
}
bool terrain_state(region::Runtime& r,const region::LocalView& view,action::TerrainState& out,uintptr_t& global,std::string& e,json* canonical_bugs=nullptr) {
    uintptr_t manager=0,release=0;
    if(!region::at(r,r.image,0x164b250,global,e)||!global||!region::at(r,global,0,out.global_random,e)||
       !region::at(r,r.image,0x13df4f0,manager,e)||!region::at(r,manager,0x38,release,e)||!region::at(r,release,0x120,out.release_edits,e))return false;
    return contour_hash(r,view.region,out.contours_sha256,e)&&bug_rows(r,view.region,out,e,canonical_bugs);
}
bool inverse(region::Runtime& r,uintptr_t graph,uintptr_t receiver,const std::string& expected,std::string& e) {
    undo::GraphWitness witness;if(!undo::read_graph(r.memory,r.image,graph,witness,e))return false;
    if(witness.operations<1)return fail(e,"terrain Undo graph has no inverse operation");
    uintptr_t array=0;if(!region::at(r,graph,0x20,array,e))return false;
    size_t found=0;
    for(int32_t i=0;i<witness.operations;++i) {
        uintptr_t operation=0,ap=0;if(!region::at(r,array,size_t(i)*8,operation,e)||!region::at(r,operation,0,ap,e))return false;
        if(ap!=r.image+0x1723330)continue;
        uintptr_t target=0;if(!region::at(r,operation,0xb8,target,e)||target!=receiver)return fail(e,"terrain inverse region differs");
        json value=json::array();for(size_t style=0;style<4;++style){action::TerrainShape shape;if(!poly::read(r.memory,operation+0xc0+style*0x70,shape,e))return false;value.push_back(shape_json(shape));}
        std::string hash;if(!region::sha256(value.dump(),hash,e))return false;
        if(hash!=expected)return fail(e,"terrain inverse contours differ from original before state");++found;
    }
    return found==1||fail(e,"terrain Undo requires exactly one original inverse contour operation");
}
void rejected(const std::string& e){++terrain.rejected;if(terrain.cause!=e){terrain.cause=e;LOG_ERROR("[UITerrain] Rejected cause={}",e);}}
bool native_error(uint32_t code,const char* operation,std::string& e) {
    return !code||fail(e,std::string("terrain original ")+operation+" SEH="+std::to_string(code));
}
void press_entry(void*,void*,const void*);void start_entry(void*,const void*);void paint_entry(void*,const void*);
void end_entry(void*);void finished_entry(void*,const void*);void cancel_entry(void*,void*);
bool native_code(region::Runtime& r,bool hooked,std::string& e) {
    const std::array<uintptr_t,6> originals{uintptr_t(terrain.press),uintptr_t(terrain.start),uintptr_t(terrain.paint),uintptr_t(terrain.end),uintptr_t(terrain.finished),uintptr_t(terrain.cancel)};
    const std::array<uintptr_t,6> entries{uintptr_t(&press_entry),uintptr_t(&start_entry),uintptr_t(&paint_entry),uintptr_t(&end_entry),uintptr_t(&finished_entry),uintptr_t(&cancel_entry)};
    for(size_t i=0;i<std::size(terrain_codes);++i){const auto& code=terrain_codes[i];std::vector<uint8_t> bytes(code.size);
        if(!r.memory.copy(r.image+code.rva,bytes.data(),bytes.size(),e))return false;
        if(!hooked||i>=6){if(std::memcmp(bytes.data(),code.bytes,code.size))return fail(e,"terrain exact native body differs RVA="+std::to_string(code.rva));continue;}
        if(bytes[0]!=0xe9||std::memcmp(bytes.data()+5,code.bytes+5,code.size-5))return fail(e,"terrain owned target/tail differs RVA="+std::to_string(code.rva));
        std::vector<uint8_t> prefix(code.span);if(!r.memory.copy(originals[i],prefix.data(),prefix.size(),e)||std::memcmp(prefix.data(),code.bytes,code.span))return fail(e,"terrain original trampoline prefix differs");
        constexpr std::array<uint8_t,6> jump{0xff,0x25,0,0,0,0};std::array<uint8_t,6> actual{};uintptr_t continuation=0;
        if(!r.memory.copy(originals[i]+code.span,actual.data(),actual.size(),e)||actual!=jump||!r.memory.copy(originals[i]+code.span+6,&continuation,8,e)||continuation!=r.image+code.rva+code.span)return fail(e,"terrain original trampoline continuation differs");
        int32_t displacement=0;std::memcpy(&displacement,bytes.data()+1,4);const auto relay=uintptr_t(int64_t(r.image+code.rva+5)+displacement);uintptr_t entry=0;
        if(!r.memory.copy(relay,actual.data(),actual.size(),e)||actual!=jump||!r.memory.copy(relay+6,&entry,8,e)||entry!=entries[i])return fail(e,"terrain original target relay owner differs");
    }return true;
}
uint32_t discard_capture(uintptr_t image,void* brush,uintptr_t graph) {
    __try {
        *reinterpret_cast<uintptr_t*>(static_cast<unsigned char*>(brush)+0x658)=0;
        terrain.end(brush);
        if(graph){reinterpret_cast<Unary>(image+0x540b90)(reinterpret_cast<void*>(graph));reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(reinterpret_cast<void*>(graph),0x110);}return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
void end_capture(void* brush,uint8_t finish) {
    std::string e;auto* r=runtime(e);if(!r){rejected(e);return;}
    if(!terrain.capture||terrain.capture->brush!=uintptr_t(brush)){terrain.end(brush);return;}
    auto& held=*terrain.capture;held.intent.finish=finish;
    if(held.cleanup_attempted){rejected("terrain capture cleanup already attempted; original result unresolved");return;}
    uintptr_t actual_graph=0,edit=0;
    if(!region::at(*r,uintptr_t(brush),0x658,actual_graph,e)||!region::at(*r,uintptr_t(brush),0x670,edit,e)||!actual_graph||!edit||actual_graph!=held.graph){rejected(e.empty()?"terrain capture original graph/edit ownership differs":e);return;}
    held.cleanup_attempted=true;
    const auto seh=discard_capture(r->image,brush,held.graph);
    undo::UndoWitness after;
    if(!native_error(seh,"capture EndEdit/graph destruction",e)||!undo::read_undo(r->memory,r->image,after,e)||after!=held.before_undo){rejected(e.empty()?"terrain capture changed native Undo history":e);return;}
    auto stroke=std::move(held);terrain.capture.reset();
    if(!stroke.cause.empty()){rejected(stroke.cause);return;}
    if(!action::valid_terrain(stroke.intent)){rejected("terrain captured stroke invalid");return;}
    if(terrain.pending){rejected("terrain completed stroke already pending");return;}
    terrain.pending=std::move(stroke);++terrain.captured;
}
void press_entry(void* brush,void* cursor,const void* position) {
    if(!terrain.ready||!net::session_wire::connected()){terrain.press(brush,cursor,position);return;}
    std::string e;auto* r=runtime(e);const auto state=typed_action_runtime::status();
    if(!r||!state.value("started",false)||state.value("locked",true)||state.value("pending",true)||terrain.capture||terrain.pending){rejected(e.empty()?"terrain UI requires idle bound Controller":e);return;}
    if(!undo::read_undo(r->memory,r->image,terrain.expected_undo,e)||terrain.expected_undo.active_graph){rejected(e.empty()?"terrain capture requires idle original Undo graph":e);return;}
    terrain.expected=uintptr_t(brush);terrain.press(brush,cursor,position);terrain.expected=0;
    // Press resets area after PaintLogicPress. Its tail Drag uses the same point,
    // so PaintLogic's distance guard cannot emit another stamp at that boundary.
    if(terrain.capture&&terrain.capture->brush==uintptr_t(brush))terrain.capture->intent.reset_after=uint32_t(terrain.capture->intent.stamps.size());
}
void start_entry(void* brush,const void* label) {
    if(!terrain.ready||terrain.expected!=uintptr_t(brush)){terrain.start(brush,label);return;}
    std::string e;auto* r=runtime(e);uintptr_t receiver=0;region::LocalView before;
    // Once original Press enters StartEdit, retain the real lifecycle even when
    // capture validation fails. AddPaint remains intercepted and has no effect.
    terrain.capture.emplace();auto& stroke=*terrain.capture;
    stroke.brush=uintptr_t(brush);stroke.before_undo=terrain.expected_undo;
    stroke.ui_token=terrain.ui_token;
    if(!r||!region::at(*r,stroke.brush,0x218,receiver,e)||!region::at(*r,receiver,0x834,stroke.intent.id,e)||
       !region::at(*r,stroke.brush,0x150,stroke.intent.style,e)||!region::at(*r,stroke.brush,0x664,stroke.intent.initial_area,e)||!region::text(*r,uintptr_t(label),stroke.intent.start_label,e)||
       !region::identity(*r,stroke.intent.id,receiver,before,e))stroke.cause=e.empty()?"terrain capture identity unavailable":e;
    else{stroke.binding={before.world.origin.world_epoch,before.world.origin.generation};stroke.snapshot=net::current_snapshot_id();}
    terrain.start(brush,label);
    if(r&&(!region::at(*r,uintptr_t(brush),0x658,stroke.graph,e)||!stroke.graph))stroke.cause=e.empty()?"terrain capture has no original graph":e;
    if(!stroke.cause.empty())rejected(stroke.cause);
}
void paint_entry(void* brush,const void* shape) {
    if(!terrain.capture||terrain.capture->brush!=uintptr_t(brush)){terrain.paint(brush,shape);return;}
    auto& stroke=*terrain.capture;if(!stroke.cause.empty())return;
    std::string e;auto* r=runtime(e);action::TerrainStamp stamp;
    if(!r||!region::at(*r,uintptr_t(brush),0x208,stamp.position,e)||!region::at(*r,uintptr_t(brush),0x660,stamp.radius,e)||!poly::read(r->memory,uintptr_t(shape),stamp.shape,e)){stroke.cause=e;rejected(e);return;}
    stroke.intent.stamps.push_back(std::move(stamp));
}
void end_entry(void* brush){if(terrain.capture&&terrain.capture->brush==uintptr_t(brush))end_capture(brush,0);else terrain.end(brush);}
void finished_entry(void* brush,const void* shape){if(terrain.capture&&terrain.capture->brush==uintptr_t(brush))end_capture(brush,1);else terrain.finished(brush,shape);}
void cancel_entry(void* brush,void* cursor){if(terrain.capture&&terrain.capture->brush==uintptr_t(brush))end_capture(brush,2);else terrain.cancel(brush,cursor);}
uint32_t start_private(uintptr_t image,TerrainOperation& operation,uintptr_t receiver,const region::NativeString& label) {
    __try {
        reinterpret_cast<Unary>(image+0x5e0ed0)(operation.brush->bytes);operation.constructed=true;
        *reinterpret_cast<uintptr_t*>(operation.brush->bytes+0x218)=receiver;
        *reinterpret_cast<int32_t*>(operation.brush->bytes+0x150)=operation.request.terrain.style;
        *reinterpret_cast<float*>(operation.brush->bytes+0x664)=operation.request.terrain.initial_area;
        terrain.start(operation.brush->bytes,&label);
        operation.graph=*reinterpret_cast<uintptr_t*>(operation.brush->bytes+0x658);
        operation.native_cost=reinterpret_cast<int32_t(*)(void*)>(image+0x5dfdf0)(operation.brush->bytes);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t stamp_private(TerrainOperation& operation,const action::TerrainStamp& stamp,const poly::Storage& shape) {
    __try {
        std::memcpy(operation.brush->bytes+0x208,stamp.position.data(),12);
        *reinterpret_cast<float*>(operation.brush->bytes+0x660)=stamp.radius;
        terrain.paint(operation.brush->bytes,&shape);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t finish_private(TerrainOperation& operation) {
    __try {
        operation.changed_area=*reinterpret_cast<float*>(operation.brush->bytes+0x664);
        if(operation.request.terrain.finish==1)terrain.finished(operation.brush->bytes,nullptr);
        else if(operation.request.terrain.finish==2)terrain.cancel(operation.brush->bytes,nullptr);
        else terrain.end(operation.brush->bytes);
        operation.returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t seed(uintptr_t region_random,uintptr_t global_random,const action::Evidence& authority) {
    __try{std::memcpy(reinterpret_cast<void*>(region_random),authority.region_before.random.data(),32);std::memcpy(reinterpret_cast<void*>(global_random),authority.terrain_before.global_random.data(),32);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}

#include "ui_terrain_height.inc"
}
bool enable(std::string& e) {
    auto* r=runtime(e);if(!r)return false;
    if(terrain.attempted)return fail(e,"terrain hook installation already attempted");terrain.attempted=true;
    if(!native_code(*r,false,e))return false;
    struct Hook{uintptr_t rva;void* replacement;void** original;};
    const Hook hooks[]{
        {0x5e0700,reinterpret_cast<void*>(&press_entry),reinterpret_cast<void**>(&terrain.press)},
        {0x5e03b0,reinterpret_cast<void*>(&start_entry),reinterpret_cast<void**>(&terrain.start)},
        {0x5dfe70,reinterpret_cast<void*>(&paint_entry),reinterpret_cast<void**>(&terrain.paint)},
        {0x5e04b0,reinterpret_cast<void*>(&end_entry),reinterpret_cast<void**>(&terrain.end)},
        {0x5e0c20,reinterpret_cast<void*>(&finished_entry),reinterpret_cast<void**>(&terrain.finished)},
        {0x5e06a0,reinterpret_cast<void*>(&cancel_entry),reinterpret_cast<void**>(&terrain.cancel)}};
    for(const auto& h:hooks){const auto status=MH_CreateHook(reinterpret_cast<void*>(r->image+h.rva),h.replacement,h.original);if(status!=MH_OK||!*h.original)return fail(e,"terrain MH_CreateHook RVA="+std::to_string(h.rva)+" status="+std::to_string(status));}
    for(const auto& h:hooks){const auto status=MH_EnableHook(reinterpret_cast<void*>(r->image+h.rva));if(status!=MH_OK)return fail(e,"terrain MH_EnableHook RVA="+std::to_string(h.rva)+" status="+std::to_string(status));}
    if(!native_code(*r,true,e)||!height_enable(*r,e))return false;terrain.ready=true;return true;
}
void tick() {
    if(!terrain.ready)return;
    const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:terrain.ui_jobs){(void)token;
        const auto id=job.value("action_uuid",std::string{});
        if(id.empty()||job.value("confirmed",false)||id!=controller.value("action_uuid",std::string{}))continue;
        if(controller.value("locked",false)){job["cause"]=controller.value("cause",std::string{"terrain Controller locked"});continue;}
        const auto operation=terrain.operations.find(id);
        if(!controller.value("pending",true)&&controller.value("outcome",0)==1&&operation!=terrain.operations.end()&&operation->second.confirmed&&operation->second.retired)job["confirmed"]=true;
    }
    if(!terrain.pending)return;
    std::string e;auto* r=runtime(e);if(!r){rejected(e);return;}
    auto stroke=std::move(*terrain.pending);terrain.pending.reset();region::LocalView before;
    if(!region::identity(*r,stroke.intent.id,0,before,e)||!region::bound(before,stroke.binding,stroke.snapshot,e)){rejected(e);return;}
    const auto id=region::uuid(e);if(id.empty()){rejected(e);return;}
    terrain.last_action=id;
    if(!stroke.ui_token.empty())terrain.ui_jobs.at(stroke.ui_token)["action_uuid"]=id;
    if(!typed_action_runtime::submit_terrain(id,stroke.intent,e)){if(!stroke.ui_token.empty())terrain.ui_jobs.at(stroke.ui_token)["cause"]=e;rejected(e);return;}
    if(!stroke.ui_token.empty())terrain.ui_jobs.at(stroke.ui_token)["submitted"]=true;++terrain.submitted;
}
bool unresolved(){if(terrain.capture||terrain.pending)return true;for(const auto& [token,job]:terrain.ui_jobs){(void)token;if(!job.value("confirmed",false))return true;}return false;}
#include "terrain_history_runtime.inc"
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e) {
    if(request.terrain.height)return height_perform(binding,request,authority,e);
    auto* r=runtime(e);if(!r||!terrain.ready||request.action!=action::Action::TerrainStroke||!action::valid_terrain(request.terrain)){e="terrain native runtime or intent unavailable: "+e;return action::Outcome::RejectedNoEffect;}
    if(terrain.operations.contains(request.action_uuid)){e="terrain action already attempted; no replay";return action::Outcome::Unknown;}
    if(!native_code(*r,true,e))return action::Outcome::RejectedNoEffect;
    const auto controller=typed_action_runtime::status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="terrain lacks exact Controller request";return action::Outcome::RejectedNoEffect;}
    region::LocalView before;action::TerrainState terrain_before;action::Ledger money;uintptr_t global=0;undo::UndoWitness undo_before;
    if(!region::read(*r,request.terrain.id,0,before,e)||!region::bound(before,binding,request.snapshot_uuid,e)||!region::ledger(money,e)||
       !terrain_state(*r,before,terrain_before,global,e)||!undo::read_undo(r->memory,r->image,undo_before,e))return action::Outcome::RejectedNoEffect;
    if(undo_before.active_graph){e="terrain requires idle Undo graph";return action::Outcome::RejectedNoEffect;}
    if(!action::terrain_receipt_fits(request,money,before.state,e))return action::Outcome::RejectedNoEffect;
    json bindings;if(!remove_diagnostics::bindings(bindings,e))return action::Outcome::RejectedNoEffect;
    if(authority&&(money!=authority->before||!action::same_region_content(before.state,authority->region_before)||!action::same_terrain_content(terrain_before,authority->terrain_before))){e="terrain client before content differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& operation=terrain.operations[request.action_uuid];operation.binding=binding;operation.request=request;
    operation.evidence.before=money;operation.evidence.region_before=authority?authority->region_before:before.state;
    operation.evidence.terrain_before=authority?authority->terrain_before:terrain_before;
    if(!history_capture(*r,before,bindings,operation,e))return action::Outcome::RejectedNoEffect;
    operation.attempted=true;
    auto unknown=[&]{operation.cause=e;return action::Outcome::Unknown;};
    if(authority&&!native_error(seed(before.random,global,*authority),"authority RNG seed",e))return unknown();
    region::NativeString label{request.terrain.start_label.data(),request.terrain.start_label.size(),{}};
    if(!native_error(start_private(r->image,operation,before.region,label),"StartEdit",e)||!inverse(*r,operation.graph,before.region,terrain_before.contours_sha256,e))return unknown();
    size_t stamp_index=0;
    for(const auto& stamp:request.terrain.stamps) {
        if(stamp_index++==request.terrain.reset_after)*reinterpret_cast<float*>(operation.brush->bytes+0x664)=0;
        poly::Storage shape;poly::BorrowedPaths paths(stamp.shape);bool constructed=false;
        auto seh=shape_make(r->image,shape,paths.view,constructed);action::TerrainShape actual;
        bool accepted=!seh&&poly::read(r->memory,uintptr_t(&shape),actual,e)&&actual==stamp.shape;
        if(accepted)seh=stamp_private(operation,stamp,shape);
        const auto cleanup=constructed?unary(reinterpret_cast<Unary>(r->image+0xf15a0),&shape):0;
        if(!native_error(seh,"PolyShape/AddPaint",e)||!native_error(cleanup,"PolyShape destructor",e))return unknown();
        if(!accepted){e="terrain original PolyShape reconstruction differs: "+e;return unknown();}
    }
    if(stamp_index==request.terrain.reset_after)*reinterpret_cast<float*>(operation.brush->bytes+0x664)=0;
    if(!native_error(finish_private(operation),"stroke completion",e))return unknown();
    uintptr_t remaining_graph=0,remaining_edit=0;undo::UndoWitness undo_after;undo::GraphWitness graph;
    if(!region::at(*r,uintptr_t(operation.brush->bytes),0x658,remaining_graph,e)||!region::at(*r,uintptr_t(operation.brush->bytes),0x670,remaining_edit,e)||
       !undo::read_undo(r->memory,r->image,undo_after,e)||!undo::read_graph(r->memory,r->image,operation.graph,graph,e)||
       !undo::validate_undo_commit(undo_before,undo_after,graph,e)||!inverse(*r,operation.graph,before.region,terrain_before.contours_sha256,e))return unknown();
    if(remaining_graph||remaining_edit){e="terrain original completion retained cursor graph/edit";return unknown();}
    operation.evidence.undo_confirmed=true;
    region::LocalView after;uintptr_t after_global=0;
    if(!region::read(*r,request.terrain.id,before.region,after,e)||!region::bound(after,binding,request.snapshot_uuid,e)||!region::ledger(operation.evidence.after,e)||
       !terrain_state(*r,after,operation.evidence.terrain_after,after_global,e))return unknown();
    operation.evidence.region_after=after.state;
    std::set_difference(before.scenery_uids.begin(),before.scenery_uids.end(),after.scenery_uids.begin(),after.scenery_uids.end(),std::back_inserter(operation.removed_scenery_uids));
    if(!remove_diagnostics::retire_region_scenery(request.snapshot_uuid,request.action_uuid,operation.removed_scenery_uids,operation.keys,e)||
       !region::sha256(json(operation.keys).dump(),operation.evidence.region_retired_sha256,e))return unknown();
    if(operation.native_cost!=0||operation.evidence.after!=money||after.state.preset!=before.state.preset||after.state.styles!=before.state.styles||
       (authority&&(operation.evidence.region_after!=authority->region_after||operation.evidence.terrain_after!=authority->terrain_after||operation.evidence.region_retired_sha256!=authority->region_retired_sha256))) {
        e="terrain original cost/content/side effects differ from authority";return unknown();
    }
    if(!history_source(*r,before,after,operation,e))return unknown();
    operation.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& b,const action::Request& request,action::Evidence& out,std::string& e) {
    if(request.terrain.height)return height_observe(b,request,out,e);
    auto* r=runtime(e);if(!r)return false;const auto found=terrain.operations.find(request.action_uuid);
    if(found==terrain.operations.end()||found->second.binding!=b||found->second.request!=request||!found->second.confirmed)return fail(e,"terrain observation lacks confirmed exact operation");
    const auto& operation=found->second;region::LocalView actual;action::TerrainState state;action::Ledger money;uintptr_t global=0;
    if(!region::read(*r,request.terrain.id,0,actual,e)||!region::bound(actual,b,request.snapshot_uuid,e)||!terrain_state(*r,actual,state,global,e)||!region::ledger(money,e))return false;
    if(actual.state!=operation.evidence.region_after||state!=operation.evidence.terrain_after||money!=operation.evidence.after)return fail(e,"terrain observation changed after original return");
    out=operation.evidence;out.retirement_confirmed=operation.retired;return true;
}
bool retire(const action::Binding& b,const action::Request& request,std::string& e) {
    action::Evidence out;if(!observe(b,request,out,e))return false;
    auto& operation=terrain.operations.at(request.action_uuid);auto* r=runtime(e);if(!r)return false;
    if(operation.retired)return fail(e,"terrain receipt already retired");
    if(!request.terrain.height&&!building_history_slice::remember_terrain(request.action_uuid,operation.graph,e))return false;
    if(!native_error(unary(reinterpret_cast<Unary>(r->image+(request.terrain.height?0x5e12a0:0x5e0fd0)),operation.brush->bytes),"private brush destructor",e))return false;
    operation.constructed=false;
    operation.retired=true;return true;
}
bool retired_keys(const action::Binding& b,const action::Request& request,std::vector<std::string>& keys,std::string& e) {
    action::Evidence out;if(!observe(b,request,out,e))return false;keys=terrain.operations.at(request.action_uuid).keys;return true;
}
json status() {
    json operations=json::array();for(const auto& [id,o]:terrain.operations)operations.push_back({{"action_uuid",id},{"attempted",o.attempted},{"returned",o.returned},{"confirmed",o.confirmed},{"retired",o.retired},{"native_cost",o.native_cost},{"height",o.request.terrain.height?json{{"up",o.request.terrain.height->up},{"cell",o.request.terrain.height->cell},{"hit",o.request.terrain.height->hit},{"picked",o.request.terrain.height->picked},{"before_sha256",o.evidence.terrain_before.heights_sha256},{"after_sha256",o.evidence.terrain_after.heights_sha256},{"before_bits",o.evidence.terrain_before.height_bits},{"after_bits",o.evidence.terrain_after.height_bits},{"undo_unchanged",o.evidence.undo_confirmed}}:json(nullptr)},{"changed_area",o.changed_area},{"undo_confirmed",o.evidence.undo_confirmed},{"removed_scenery_uids",o.removed_scenery_uids},{"retired_keys",o.keys},{"release_before",o.evidence.terrain_before.release_edits},{"release_after",o.evidence.terrain_after.release_edits},{"bugs_before",o.evidence.terrain_before.bug_count},{"bugs_after",o.evidence.terrain_after.bug_count},{"cause",o.cause}});
    return {{"enabled",terrain.ready},{"capturing",bool(terrain.capture)},{"pending",bool(terrain.pending)},{"captured",terrain.captured},{"submitted",terrain.submitted},{"rejected",terrain.rejected},{"last_action_uuid",terrain.last_action},{"cause",terrain.cause},{"operations",operations},{"ui_jobs",terrain.ui_jobs}};
}
bool query(int32_t id,json& out,std::string& e) {
    auto* r=runtime(e);if(!r)return false;region::LocalView view;action::TerrainState state;uintptr_t global=0;
    if(!region::read(*r,id,0,view,e)||!terrain_state(*r,view,state,global,e))return false;
    out=region::state_json(view.state);out["contours_sha256"]=state.contours_sha256;out["bugs_sha256"]=state.bugs_sha256;out["bug_count"]=state.bug_count;out["release_edits"]=state.release_edits;out["global_random"]=state.global_random;terrain_height::Grid heights;std::string height_sha;if(!height_grid(*r,view.region,heights,height_sha,e))return false;out["heights_sha256"]=height_sha;out["height_grid"]={{"width",heights.width},{"height",heights.height},{"origin",heights.origin},{"cell_size",heights.cell_size}};return true;
}
namespace {
uint32_t input_call(uintptr_t image,unsigned step,const placement_ui_probe::Binding& b,const std::array<float,3>& point) {
    __try {
        auto* brush=reinterpret_cast<void*>(b.behaviour);auto* cursor=reinterpret_cast<void*>(b.cursor);
        if(step==0)reinterpret_cast<Unary>(image+0x580870)(reinterpret_cast<void*>(b.button));
        else if(step==1)reinterpret_cast<Press>(image+0x5e0800)(brush,cursor,point.data());
        else if(step==2)reinterpret_cast<Press>(image+0x5e0700)(brush,cursor,point.data());
        else if(step==3)reinterpret_cast<void(*)(void*,void*,const void*,float)>(image+0x5e09b0)(brush,cursor,point.data(),0.0f);
        else if(step==4)reinterpret_cast<Press>(image+0x5dfe30)(brush,cursor,point.data());
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t point_region(uintptr_t image,uintptr_t shard,const std::array<float,3>& point,uintptr_t& receiver) {
    __try{receiver=uintptr_t(reinterpret_cast<void*(*)(void*,const void*,void*)>(image+0x5089b0)(reinterpret_cast<void*>(shard),point.data(),nullptr));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool native_stroke(const json& args,json& out,std::string& e) {
    out=json::object();auto* r=runtime(e);if(!r)return false;
    if(!terrain.ready||unresolved()||placement_ui_diagnostics::unresolved())return fail(e,"terrain actual UI requires ready idle capture and no unresolved UI job");
    const auto controller=typed_action_runtime::status();
    if(!net::session_wire::connected()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"terrain actual UI requires connected idle Controller");
    if(!args.is_object()||args.size()!=4||!args.contains("token")||!args.at("token").is_string()||!args.contains("id")||!args.at("id").is_number_integer()||!args.contains("style")||!args.at("style").is_number_integer()||!args.contains("points")||!args.at("points").is_array())return fail(e,"terrain native UI requires token, id, style, points");
    const auto token=args.at("token").get<std::string>();const auto region_id=args.at("id").get<int64_t>(),style=args.at("style").get<int64_t>();
    if(token.empty()||token.size()>128||token.find('\0')!=std::string::npos||terrain.ui_jobs.contains(token)||region_id<0||region_id>INT32_MAX||style<0||style>3||args.at("points").empty()||args.at("points").size()>1024)return fail(e,"terrain native UI token/id/style/point count invalid or already attempted");
    std::vector<std::array<float,3>> points;
    for(const auto& values:args.at("points")){
        if(!values.is_array()||values.size()!=3)return fail(e,"terrain native UI point requires three finite coordinates");
        std::array<float,3> point{};for(size_t i=0;i<3;++i){if(!values[i].is_number())return fail(e,"terrain native UI coordinate is not numeric");const auto value=values[i].get<double>();if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())return fail(e,"terrain native UI coordinate outside finite float32");point[i]=float(value);}points.push_back(point);
    }
    if(!native_code(*r,true,e))return false;
    region::LocalView before;if(!region::identity(*r,int32_t(region_id),0,before,e))return false;
    uint8_t owned=0;if(!region::at(*r,before.region,0x850,owned,e)||owned!=1)return fail(e,"terrain native UI region is not owned");
    for(const auto& point:points){uintptr_t found=0;const auto seh=point_region(r->image,before.world.root,point,found);if(seh||found!=before.region)return fail(e,"terrain native UI point differs from exact region, SEH="+std::to_string(seh));}
    placement_ui_probe::Context context{r->image,uintptr_t(before.world.origin.state),before.world.origin.world_epoch,before.world.origin.generation,r->thread};
    placement_ui_probe::Request request;request.token=token;request.family=placement_ui_probe::Family::Terrain;request.terrain_style=int32_t(style);
    placement_ui_probe::ProcessMemory memory;placement_ui_probe::Binding binding;
    if(!placement_ui_probe::read_existing_tool(memory,context,request,binding,e))return false;
    auto& job=terrain.ui_jobs[token];job={{"token",token},{"attempted",0},{"returned",0},{"captured",false},{"submitted",false},{"action_uuid",""},{"cause",""}};
    terrain.ui_token=token;struct Reset{~Reset(){terrain.ui_token.clear();}} reset;
    const auto captured=terrain.captured;
    auto call=[&](unsigned step,const std::array<float,3>& point){job["attempted"]=job.at("attempted").get<unsigned>()+1;const auto seh=input_call(r->image,step,binding,point);if(seh){e="terrain original UI callback SEH="+std::to_string(seh)+" step="+std::to_string(step);job["cause"]=e;return false;}job["returned"]=job.at("returned").get<unsigned>()+1;return true;};
    bool ok=call(0,points.front());uintptr_t active=0,ap=0;
    if(ok)ok=region::at(*r,binding.cursor,0x190,active,e)&&active==binding.behaviour&&region::at(*r,active,0,ap,e)&&ap==r->image+0x173f580;
    if(ok)ok=call(1,points.front());
    if(ok)ok=call(2,points.front());
    if(ok&&(!terrain.capture||terrain.capture->brush!=binding.behaviour)){e="terrain original UI Press did not enter captured StartEdit";ok=false;}
    for(size_t i=1;ok&&i<points.size();++i)ok=call(3,points[i]);
    if(ok)ok=call(4,points.back());
    if(ok)ok=terrain.captured==captured+1&&terrain.pending&&terrain.pending->ui_token==token;
    if(!ok){if(e.empty())e="terrain original UI returned without one completed captured stroke: "+terrain.cause;job["cause"]=e;out=job;return false;}
    job["captured"]=true;job["stamps"]=terrain.pending->intent.stamps.size();out=job;return true;
}
#include "ui_terrain_height_driver.inc"
}
