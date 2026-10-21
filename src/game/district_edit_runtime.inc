namespace district_edit {
namespace {
namespace property=district_property;
namespace controls=district_property_ui;
namespace history=district_edit_history;
using Notice=void(__fastcall*)(void*,const prop_text::Text*);
using Spinner=void(__fastcall*)(void*,int32_t);
using Color=void(__fastcall*)(void*,const void*,bool);
Notice begin_original=nullptr,end_original=nullptr,cancel_original=nullptr;
Spinner spinner_original=nullptr;Color color_original=nullptr;
bool installed=false,install_attempted=false,replaying=false;
bool code(Runtime&,std::string&);
void clear_history();
std::string install_cause,cause;
struct Source {
    action::Binding binding;std::string snapshot,key;
    district_saved_identity::Current target;property::Value value;
    action::DistrictState before;action::Ledger money;witness::UndoWitness undo;
    bool dragging=false,changed=false;
};
struct Operation {
    action::Binding binding;action::Request request;district_saved_identity::Current target;
    action::Evidence evidence;witness::UndoWitness undo;uintptr_t graph=0;
    std::map<uintptr_t,history::Stored> inverse;
    uint32_t steps=0;bool attempted=false,returned=false,confirmed=false,retired=false;std::string cause;
};
std::optional<Source> source,pending_source;
std::map<std::string,Operation> operations;
uint64_t captures=0,submissions=0,rejections=0;
void reject(const std::string& error){++rejections;if(cause!=error){cause=error;LOG_ERROR("[DistrictEdit] {}",error);}}
bool active(){return installed&&!replaying&&net::session_wire::connected()&&typed_action_runtime::controller_status().value("started",false);}
bool idle(Runtime& runtime,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!runtime.ready||runtime.thread!=GetCurrentThreadId()||pending_source||runtime.pending||district_text::pending()||district_title::pending()||
       !controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true)){
        error="District authored UI requires idle bound Mainthread Controller";return false;}
    return true;
}
bool capture_owner(Runtime& runtime,uintptr_t object,property::Kind kind,Source& out,std::string& error){
    if(!idle(runtime,error)||!code(runtime,error))return false;
    out.snapshot=net::current_snapshot_id();uint64_t uid=0;host::WorldState world_value;
    if(!typed_action_runtime::network_binding(out.snapshot,out.binding,error)||!world(runtime,world_value,error)||
       !district_reader::ui(runtime.memory,runtime.image,world_value.selection,object,error)||
       !district_saved_identity::at(runtime.memory,object,0x18,uid,error)||
       !district_saved_identity_runtime::bind_local(object,uid,out.key,error)||
       !district_saved_identity_runtime::resolve(out.key,out.target,error)||
       !district_text::sample(runtime,out.binding,out.target,out.before,out.money,out.undo,error))return false;
    out.value=property::current(kind,out.before);return true;
}
bool unchanged(Runtime& runtime,const Source& value,std::string& error){
    action::DistrictState current;action::Ledger money;witness::UndoWitness undo;
    if(value.snapshot!=net::current_snapshot_id()||!district_text::sample(runtime,value.binding,value.target,current,money,undo,error))return false;
    if(current!=value.before||money!=value.money||undo!=value.undo){error="District suppressed source changed complete native state/Ledger/Undo";return false;}
    return true;
}
bool queue(Runtime& runtime,Source value,std::string& error){
    if(!unchanged(runtime,value,error))return false;
    if(!value.changed||!property::changed(value.value,value.before))return true;
    if(pending_source){error="District source already has an ordered pending edit";return false;}
    pending_source=std::move(value);++captures;return true;
}
void notice_entry(void* object,const prop_text::Text* text_value,int phase){
    auto original=phase==0?begin_original:phase==1?end_original:cancel_original;
    auto* runtime=published.load(std::memory_order_acquire);
    if(!active()||!runtime){original(object,text_value);return;}
    try{
        std::string name,error;if(!prop_text::text(*runtime,uintptr_t(text_value),name,error)){reject(error);return;}
        const auto* field=property::field(name);
        if(!field){original(object,text_value);return;}
        if(phase==0){
            if(source){reject("District native property Begin reentered another active source");return;}
            Source value;if(!capture_owner(*runtime,uintptr_t(object),field->kind,value,error)){reject(error);return;}
            value.dragging=true;source=std::move(value);return;
        }
        if(!source){reject("District native property End/Cancel lacks its captured Begin");return;}
        if(source->target.row.pointer!=uintptr_t(object)||source->value.kind!=field->kind){reject("District native property End/Cancel owner differs");return;}
        auto value=std::move(*source);source.reset();
        if(phase==2){if(!unchanged(*runtime,value,error))reject(error);return;}
        if(!queue(*runtime,std::move(value),error))reject(error);
    }catch(const std::exception& exception){reject(std::string("District native property notice: ")+exception.what());}
}
void begin_entry(void* object,const prop_text::Text* value){notice_entry(object,value,0);}
void end_entry(void* object,const prop_text::Text* value){notice_entry(object,value,1);}
void cancel_entry(void* object,const prop_text::Text* value){notice_entry(object,value,2);}
bool capture_widget(Runtime& runtime,const controls::Binding& binding,const property::Value& value,controls::Widget kind,std::string& error){
    if(!property::valid(value)||binding.kind!=value.kind){error="District captured property input invalid";return false;}
    if(source){
        if(source->target.row.pointer!=binding.object||source->target.row.uid!=binding.uid||source->value.kind!=value.kind||!source->dragging){
            error="District native widget changed active property edit owner";return false;}
        if(!unchanged(runtime,*source,error)||!controls::unchanged(runtime.memory,runtime.image,binding,kind,error))return false;
        source->value=value;source->changed=property::changed(value,source->before);return true;
    }
    if(binding.editing){error="District native widget has an edit lifetime without a captured Begin";return false;}
    Source captured;if(!capture_owner(runtime,binding.object,value.kind,captured,error)||
       captured.target.row.uid!=binding.uid||!controls::unchanged(runtime.memory,runtime.image,binding,kind,error))return false;
    captured.value=value;captured.changed=property::changed(value,captured.before);
    return queue(runtime,std::move(captured),error);
}
uint32_t continent_call(uintptr_t image,uintptr_t root,uintptr_t point,float& height){
    __try{height=reinterpret_cast<float(__fastcall*)(void*,const void*)>(image+0x5083f0)(reinterpret_cast<void*>(root),reinterpret_cast<void*>(point));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool continent(Runtime& runtime,uintptr_t object,uint32_t& bits,std::string& error){
    uintptr_t root=0;if(!district_saved_identity::at(runtime.memory,object,0x170,root,error))return false;
    float height=0;const auto exception=continent_call(runtime.image,root,object+0x338,height);
    if(exception||!std::isfinite(height)){error="District original continent height failed SEH="+std::to_string(exception);return false;}
    bits=std::bit_cast<uint32_t>(height);return true;
}
uint32_t invoke(uintptr_t image,uintptr_t object,const property::Value& value,uint32_t& steps){
    const auto* field=property::field(value.kind);
    prop_text::Text name{field->name.data(),field->name.size(),{}};
    __try{
        begin_original(reinterpret_cast<void*>(object),&name);steps|=1;
        if(field->type==property::Type::Flag)*reinterpret_cast<uint8_t*>(object+field->offset)=uint8_t(value.bits[0]);
        else std::memcpy(reinterpret_cast<void*>(object+field->offset),value.bits.data(),field->type==property::Type::Color?16:4);
        steps|=2;
        reinterpret_cast<Notice>(image+0x41d100)(reinterpret_cast<void*>(object),&name);steps|=4;
        end_original(reinterpret_cast<void*>(object),&name);steps|=8;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
#include "district_widget_runtime.inc"
#include "district_edit_routes.inc"
bool pending(){return bool(source)||bool(pending_source);}
void retire_session(){source.reset();pending_source.reset();operations.clear();clear_history();cause.clear();}
void tick(){
    if(!net::session_wire::connected()){if(pending())reject("District captured source cancelled after session retirement");retire_session();return;}
    if(!pending_source)return;
    auto value=std::move(*pending_source);pending_source.reset();auto* runtime=published.load(std::memory_order_acquire);std::string error;
    if(!runtime||!unchanged(*runtime,value,error)){reject(error.empty()?"District source runtime retired before submission":error);return;}
    const auto id=uuid(error);
    if(id.empty()||!typed_action_runtime::submit_district_property(id,value.key,value.value,error)){reject(error);return;}++submissions;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!runtime||!runtime->ready||!installed||runtime->thread!=GetCurrentThreadId()||request.action!=action::Action::EditDistrict||
       request.snapshot_uuid!=net::current_snapshot_id()||bool(authority)==Config::instance().is_host||
       !property::valid(request.district_property)||!code(*runtime,error)||!contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="District property exact request/current Controller/role unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="District property original already attempted; no replay";return action::Outcome::Unknown;}
    district_saved_identity::Current target;action::DistrictState before;action::Ledger money;witness::UndoWitness undo;uint32_t floor=0;
    if(!district_saved_identity_runtime::resolve(request.canonical_record_key,target,error)||
       !district_text::sample(*runtime,binding,target,before,money,undo,error)||!continent(*runtime,target.row.pointer,floor,error))return action::Outcome::RejectedNoEffect;
    if(authority&&(!action::valid_district_property_effect(request,*authority)||authority->district_before!=before||authority->before!=money||authority->district_continent_height!=floor)){
        error="District property Client complete current state/Ledger/continent differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::district_property_wire_fits(request,before,money,floor,error))return action::Outcome::RejectedNoEffect;
    auto& operation=operations.emplace(request.action_uuid,Operation{binding,request,target}).first->second;
    operation.evidence.before=money;operation.evidence.district_before=before;operation.evidence.district_continent_height=floor;
    auto unknown=[&](const std::string& why){error=operation.cause=why;return action::Outcome::Unknown;};
    operation.attempted=true;replaying=true;const auto exception=invoke(runtime->image,target.row.pointer,request.district_property,operation.steps);replaying=false;
    operation.returned=operation.steps==15;
    if(exception||!operation.returned)return unknown("District original property failed SEH="+std::to_string(exception)+" steps="+std::to_string(operation.steps));
    action::DistrictState after;action::Ledger after_money;witness::UndoWitness after_undo;
    if(!district_text::sample(*runtime,binding,target,after,after_money,after_undo,error))return unknown(error);
    const auto* field=property::field(request.district_property.kind);
    if(field->original_history){
        if(after_undo.undo.empty())return unknown("District original property did not create its expected Undo graph");
        operation.graph=after_undo.undo.back();witness::GraphWitness graph;history::Stored stored;uintptr_t root=0,region=0;
        if(!witness::read_graph(runtime->memory.process,runtime->image,operation.graph,graph,error)||
           !witness::validate_undo_commit(undo,after_undo,graph,error)||
           !district_saved_identity::at(runtime->memory,target.row.pointer,0x170,root,error)||
           !district_saved_identity::at(runtime->memory,target.row.pointer,0x178,region,error)||
           !history::graph(runtime->memory,runtime->image,operation.graph,stored,error)||
           stored!=history::stored(target.row.uid,region,root,before))return unknown(error.empty()?"District original inverse payload differs from complete before state":error);
        operation.inverse.emplace(operation.graph,std::move(stored));operation.evidence.undo_confirmed=true;
    }else if(undo!=after_undo)return unknown("District property without native history changed Undo");
    operation.undo=after_undo;operation.evidence.after=after_money;operation.evidence.district_after=after;
    if(!action::valid_district_property_effect(request,operation.evidence)||
       (authority&&(authority->district_after!=after||authority->after!=after_money)))return unknown("District original complete property result differs from request/Host");
    operation.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!runtime||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){error="District property observation lacks exact confirmed operation";return false;}
    const auto& operation=found->second;action::DistrictState current;action::Ledger money;witness::UndoWitness undo;
    if(!district_text::sample(*runtime,binding,operation.target,current,money,undo,error))return false;
    if(current!=operation.evidence.district_after||money!=operation.evidence.after||undo!=operation.undo){error="District property current state/Ledger/Undo differs before ACK";return false;}
    if(operation.graph){history::Stored stored;
        if(!history::graph(runtime->memory,runtime->image,operation.graph,stored,error)||stored!=operation.inverse.at(operation.graph)){
            if(error.empty())error="District property original inverse changed before ACK";return false;}}
    out=operation.evidence;out.retirement_confirmed=operation.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence current;if(!observe(binding,request,current,error))return false;
    auto& operation=operations.at(request.action_uuid);if(operation.retired){error="District property already retired";return false;}
    if(operation.graph&&!building_history_slice::remember_district(request.action_uuid,operation.graph,error))return false;
    operation.retired=true;return true;
}
#include "district_edit_history_runtime.inc"
namespace {void clear_history(){history_operations.clear();}}
json status(){
    json values=json::object();for(const auto& [id,operation]:operations)values[id]={{"attempted",operation.attempted},{"returned",operation.returned},
        {"confirmed",operation.confirmed},{"retired",operation.retired},{"steps",operation.steps},{"cause",operation.cause}};
    return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submissions},{"rejections",rejections},{"cause",cause},{"operations",std::move(values)}};
}
}
