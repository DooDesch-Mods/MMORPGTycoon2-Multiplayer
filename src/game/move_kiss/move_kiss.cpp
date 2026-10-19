#include "../monster_target_key.h"
#include "move_kiss.h"
#include "../npc_target_key.h"
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>

namespace mt2mp::move_kiss {
namespace {
bool fail(std::string& e,const char* cause){e=cause;return false;}
template<class T> bool read(Memory& m,uintptr_t base,size_t offset,T& value,std::string& e){
    if(base==0 || base>std::numeric_limits<uintptr_t>::max()-offset)return fail(e,"move_read_address_range");
    return m.read(base+offset,&value,sizeof(value),e);
}
bool live(const State& s,bool npc=false){
    if(!s.alive||(!npc&&(s.type.empty()||s.variant.empty()))||
       (npc&&(!s.type.empty()||!s.variant.empty()))||
       s.type.size()>256||s.variant.size()>256||
       s.type.find('\0')!=std::string::npos||s.variant.find('\0')!=std::string::npos)return false;
    if(!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z))return false;
    for(float f:s.rotation)if(!std::isfinite(f))return false;
    for(float f:s.scale)if(!std::isfinite(f)||(npc&&f<=0))return false;
    return true;
}
}
bool Capture::read_intent(uintptr_t b,const Origin& o,Intent& out,std::string& e){
    uintptr_t ap=0,records=0; int32_t n=0,cap=0; uint8_t pending=0,duplicate=0;
    if(!read(memory_,b,0,ap,e)||ap!=image_+0x172f1f0)return e.empty()?fail(e,"move_behaviour_addresspoint"):false;
    if(!read(memory_,b,0x401,pending,e)||pending!=1)return e.empty()?fail(e,"move_commit_not_pending"):false;
    if(!read(memory_,b,0x499,duplicate,e)||duplicate!=0)return e.empty()?fail(e,"move_duplicate_not_supported"):false;
    if(!read(memory_,b,0x458,ap,e)||ap!=image_+0x1747300)return e.empty()?fail(e,"move_record_array_addresspoint"):false;
    if(!read(memory_,b,0x460,records,e)||!read(memory_,b,0x468,n,e)||!read(memory_,b,0x46c,cap,e))return false;
    if(n!=1||cap<n||records==0)return fail(e,"move_requires_single_record");
    out.origin=o;out.behaviour=b;out.record=records;
    if(!read(memory_,records,8,out.ghost,e)||!read(memory_,records,0x10,out.target,e)||!read(memory_,out.target,0x18,out.local_uid,e))return false;
    if(!out.ghost||out.ghost==out.target||!out.local_uid||out.ghost>std::numeric_limits<uintptr_t>::max()-0x16c)return fail(e,"move_ghost_target_identity");
    if(!read(memory_,out.ghost,0,ap,e)||ap!=image_+0x1705230)return e.empty()?fail(e,"move_ghost_addresspoint"):false;
    if(!services_.bind_target(o,out.ghost,out.target,out.local_uid,out.before,out.object_key,out.family,e))return false;
    const bool npc=out.family==Family::Npc||out.family==Family::DirectMonster;
    if(out.family!=Family::Prop&&!npc)return fail(e,"move_target_family_invalid");
    if(out.family==Family::Npc&&!game::npc_target_key::valid(out.object_key))return fail(e,"move_character_key_invalid");
    if(out.family==Family::DirectMonster&&!game::monster_target_key::valid(out.object_key))return fail(e,"move_monster_birth_key_required");
    if(out.object_key.empty()||!live(out.before,npc))return fail(e,"move_registry_state_invalid");
    out.after=out.before;
    if(!memory_.read(out.ghost+0x100,out.after.rotation.data(),sizeof(float)*4,e)||
       !read(memory_,out.ghost,0x110,out.after.x,e)||!read(memory_,out.ghost,0x114,out.after.y,e)||!read(memory_,out.ghost,0x118,out.after.z,e)||
       !memory_.read(out.ghost+0x11c,out.after.scale.data(),sizeof(float)*3,e))return false;
    return live(out.after,npc)||fail(e,"move_ghost_pose_nonfinite");
}
Route Capture::purchase(uintptr_t b,uintptr_t caller)noexcept{
    if(caller!=0x5c2a20||!services_.main_thread())return Route::Forward;
    Origin origin;std::string e;
    try{
        if(!services_.active(origin,e)){
            if(e.empty())return Route::Forward;
            services_.reject(e);return Route::Suppress;
        }
        if(origin.snapshot_uuid.empty()||!origin.world||!origin.world_epoch||!origin.generation){services_.reject("move_origin_incomplete");return Route::Suppress;}
        if(busy_){services_.reject("move_capture_reentry");return Route::Suppress;}
        busy_=true;
        Intent a,z;Origin current;
        if(!read_intent(b,origin,a,e)||!services_.active(current,e)||current!=origin||!read_intent(b,current,z,e)||z!=a){
            if(e.empty())e="move_capture_witness_changed";
            services_.reject(e);busy_=false;return Route::Suppress;
        }
        if(!services_.submit_move(a,e))services_.reject(e.empty()?"move_submit_rejected_without_cause":e);
        busy_=false;return Route::Suppress;
    }catch(const std::exception& x){services_.reject(std::string("move_capture_exception: ")+x.what());}
    catch(...){services_.reject("move_capture_nonstandard_exception");}
    busy_=false;return Route::Suppress;
}
static bool write_transform(const State& s,std::array<unsigned char,0x6c>& out,std::string& e,bool npc){
    if(!live(s,npc))return fail(e,"move_transform_state_invalid");
    out={};
    std::memcpy(out.data(),s.rotation.data(),16);
    std::memcpy(out.data()+0x10,&s.x,4);std::memcpy(out.data()+0x14,&s.y,4);std::memcpy(out.data()+0x18,&s.z,4);
    std::memcpy(out.data()+0x1c,s.scale.data(),12);out[0x28]=1;
    return true;
}
bool transform(const State& s,std::array<unsigned char,0x6c>& out,std::string& e){
    return write_transform(s,out,e,false);
}
bool character_transform(const State& s,std::array<unsigned char,0x6c>& out,std::string& e){return write_transform(s,out,e,true);}
bool npc_transform(const State& s,std::array<unsigned char,0x6c>& out,std::string& e){
    return write_transform(s,out,e,true);
}
action_session::Outcome execute(ExecutionServices& io,const Calls& c,const action_session::Request& req,Receipt& r,action_session::Evidence& evidence,std::string& e){
    using E=action_session::Outcome;
    if(r.started){e="move_native_already_attempted";return E::Unknown;}
    LocalBinding before;
    try{
        if(req.action!=action_session::Action::MoveBuilding||!live(req.building)){e="move_execution_request_invalid";return E::RejectedNoEffect;}
        if(!io.prepare(req,before,e)){if(e.empty())e="move_execution_prepare_failed_without_cause";return E::RejectedNoEffect;}
        if(before.origin.snapshot_uuid!=req.snapshot_uuid||!before.origin.world||!before.origin.world_epoch||!before.origin.generation||
           !before.target||!before.local_uid||!live(before.actual)||before.actual.type!=req.building.type||before.actual.variant!=req.building.variant){e="move_execution_binding_invalid";return E::RejectedNoEffect;}
        if(!transform(req.building,before.transform,e))return E::RejectedNoEffect;
        evidence.before=before.ledger;evidence.object_before=before.actual;
        if(!execute_once(c,before.target,before.transform,r,e))return r.started?E::Unknown:E::RejectedNoEffect;
        Readback after;
        if(!io.readback(before,r.graph,after,e)){if(e.empty())e="move_execution_readback_failed_without_cause";return E::Unknown;}
        if(after.current.origin!=before.origin||after.current.target!=before.target||after.current.local_uid!=before.local_uid){e="move_execution_world_or_target_changed";return E::Unknown;}
        if(after.current.actual!=req.building){e="move_execution_pose_readback_differs";return E::Unknown;}
        if(after.current.ledger!=before.ledger){e="move_execution_zero_cost_ledger_changed";return E::Unknown;}
        if(!after.manager_registered){e="move_execution_manager_registration_missing";return E::Unknown;}
        if(!after.inverse_registered){e="move_execution_inverse_registration_missing";return E::Unknown;}
        if(!after.selection_present){e="move_execution_selection_missing";return E::Unknown;}
        if(!after.edit_closed){e="move_execution_edit_not_closed";return E::Unknown;}
        evidence.after=after.current.ledger;evidence.object_after=after.current.actual;
        evidence.undo_confirmed=true;evidence.selection_confirmed=true;
        return E::Confirmed;
    }catch(const std::exception& x){e=std::string("move_execution_exception: ")+x.what();}
    catch(...){e="move_execution_nonstandard_exception";}
    return r.started?E::Unknown:E::RejectedNoEffect;
}
}
