#include "scenery_state.h"
#ifdef _WIN32
#include "live_host_state/memory_query_timing.h"
#endif
#include <cmath>
#include <exception>
#include <limits>

namespace mt2mp::scenery_kiss {
namespace {
bool fail(std::string& e,const char* cause){e=cause;return false;}
template<class T>bool at(native::Memory& m,uintptr_t p,size_t offset,T& out,std::string& e){
    if(!p||p>(std::numeric_limits<uintptr_t>::max)()-offset)return fail(e,"scenery_state_address_range");
    if(!m.copy(p+offset,&out,sizeof out,e)){if(e.empty())e="scenery_state_read_failed_without_cause";return false;}return true;
}
bool pose(native::Memory& m,uintptr_t p,State& out,std::string& e){
#ifdef _WIN32
    mt2mp::game::live_host_state::apply_timing::PageReadScope fields(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
#endif
    if(!at(m,p,0x100,out.rotation,e)||!at(m,p,0x110,out.x,e)||!at(m,p,0x114,out.y,e)||!at(m,p,0x118,out.z,e)||!at(m,p,0x11c,out.scale,e))return false;
    if(!std::isfinite(out.x)||!std::isfinite(out.y)||!std::isfinite(out.z))return fail(e,"scenery_state_position_nonfinite");
    for(float v:out.rotation)if(!std::isfinite(v))return fail(e,"scenery_state_rotation_nonfinite");
    for(float v:out.scale)if(!std::isfinite(v))return fail(e,"scenery_state_scale_nonfinite");
    return true;
}
bool identify(Catalog& c,int32_t category,int32_t variant,State& s,std::string& e){
    if(category<0||category>=47)return fail(e,"scenery_state_category_out_of_range");
    if(!c.identify(category,variant,s.type,s.variant,e)){if(e.empty())e="scenery_catalog_failed_without_cause";return false;}
    for(const auto* text:{&s.type,&s.variant})if(text->empty()||text->size()>256||text->find('\0')!=std::string::npos)return fail(e,"scenery_catalog_identifier_invalid");
    s.alive=true;return true;
}
struct View {Binding binding;State state;int32_t category=0,variant=0;bool operator==(const View&)const=default;};
}
bool valid_state(const State& s,std::string& e){
    for(const auto* text:{&s.type,&s.variant})if(text->empty()||text->size()>256||text->find('\0')!=std::string::npos)return fail(e,"scenery_catalog_identifier_invalid");
    if(!s.alive)return fail(e,"scenery_state_not_alive");
    if(!std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.z))return fail(e,"scenery_state_position_nonfinite");
    for(float v:s.rotation)if(!std::isfinite(v))return fail(e,"scenery_state_rotation_nonfinite");
    for(float v:s.scale)if(!std::isfinite(v))return fail(e,"scenery_state_scale_nonfinite");return true;
}
bool read_state(native::Memory& memory,Catalog& catalog,Root& root,uintptr_t image,uint64_t uid,State& out,std::string& e){
    out={};e.clear();
    if(!uid||!image||image>(std::numeric_limits<uintptr_t>::max)()-0x16c1390)return fail(e,"scenery_state_image_or_uid_invalid");
    try{
        auto read=[&](View& v){
            if(!root.bind(uid,v.binding,e))return false;
            const auto& b=v.binding;
            if(b.uid!=uid||!b.object||!b.epoch||!b.generation||!b.world||!b.shard||!b.region)return fail(e,"scenery_state_root_binding_incomplete");
            uintptr_t ap=0,shard=0,region=0;uint64_t actual_uid=0;
            {
#ifdef _WIN32
    mt2mp::game::live_host_state::apply_timing::PageReadScope fields(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());
#endif
            if(!at(memory,b.object,0,ap,e)||!at(memory,b.object,0x18,actual_uid,e)||!at(memory,b.object,0x170,shard,e)||!at(memory,b.object,0x178,region,e)||!at(memory,b.object,0x268,v.category,e)||!at(memory,b.object,0x26c,v.variant,e))return false;
            }
            if(ap!=image+0x16c1390||actual_uid!=uid||shard!=b.shard||region!=b.region)return fail(e,"scenery_state_native_binding_differs");
            return identify(catalog,v.category,v.variant,v.state,e)&&pose(memory,b.object,v.state,e);
        };
        View a,b;if(!read(a)||!read(b))return false;
        if(a!=b)return fail(e,"scenery_state_changed_during_read");out=a.state;return true;
    }catch(const std::exception& x){e=std::string("scenery_state_exception: ")+x.what();return false;}
    catch(...){return fail(e,"scenery_state_nonstandard_exception");}
}
Capture read_single_intent(native::Memory& m,Catalog& catalog,uintptr_t image,uintptr_t root_shard,uintptr_t receiver,State& out,std::string& e){
    out={};e.clear();
    if(!root_shard){e="scenery_capture_actual_root_shard_missing";return Capture::Rejected;}
    try{
        struct Intent {uintptr_t receiver_ap=0,ghost=0,ghost_shard=0,blueprint=0,options=0,options_mode_data=0;uint64_t ghost_uid=0;size_t options_mode_size=0;int32_t mode=0,category=0,variant=0;std::array<unsigned char,3> flags{};std::array<char,6> options_mode{};State state;bool operator==(const Intent&)const=default;};
        auto read=[&](Intent& i){
            if(!at(m,receiver,0,i.receiver_ap,e)||i.receiver_ap!=image+0x17396b0)return fail(e,"scenery_capture_receiver_class_differs");
            if(!at(m,receiver,0x174,i.mode,e)||!at(m,receiver,0x6c0,i.flags,e))return false;
            if(i.mode!=0)return fail(e,"scenery_capture_bulk_mode_unimplemented");
            if(i.flags[0]||i.flags[1]!=1||i.flags[2])return fail(e,"scenery_capture_pending_flags_invalid");
            if(!at(m,receiver,0xb58,i.ghost,e)||!at(m,receiver,0xb50,i.blueprint,e)||!at(m,receiver,0xb68,i.options,e)||!at(m,receiver,0x178,i.category,e)||!at(m,receiver,0x17c,i.variant,e))return false;
            uintptr_t ghost_ap=0,bp_ap=0,owned_blueprint=0;
            if(!i.ghost||!i.blueprint||!i.options||!at(m,i.ghost,0,ghost_ap,e)||!at(m,i.blueprint,0,bp_ap,e)||!at(m,i.ghost,0x210,owned_blueprint,e))return false;
            if(ghost_ap!=image+0x1705230||bp_ap!=image+0x16dd870||owned_blueprint!=i.blueprint)return fail(e,"scenery_capture_owned_blueprint_differs");
            if(!at(m,i.ghost,0x18,i.ghost_uid,e)||!at(m,i.ghost,0x170,i.ghost_shard,e))return false;
            if(i.ghost_shard!=root_shard)return fail(e,"scenery_capture_ghost_root_shard_differs");
            uintptr_t options_ap=0;
            if(!at(m,i.options,0,options_ap,e)||!at(m,i.options,0x88,i.options_mode_data,e)||!at(m,i.options,0x90,i.options_mode_size,e))return false;
            if(options_ap!=image+0x1715390||!i.options_mode_data||i.options_mode_size!=6)return fail(e,"scenery_capture_options_single_mode_required");
            if(!m.copy(i.options_mode_data,i.options_mode.data(),i.options_mode.size(),e))return false;
            if(i.options_mode!=std::array<char,6>{'s','i','n','g','l','e'})return fail(e,"scenery_capture_options_single_mode_required");
            return identify(catalog,i.category,i.variant,i.state,e)&&pose(m,i.ghost,i.state,e);
        };
        uintptr_t ap=0;int32_t mode=0;std::array<unsigned char,3> flags{};
        if(!at(m,receiver,0,ap,e)||ap!=image+0x17396b0){if(e.empty())e="scenery_capture_receiver_class_differs";return Capture::Rejected;}
        if(!at(m,receiver,0x174,mode,e))return Capture::Rejected;
        if(mode!=0){e="scenery_capture_bulk_mode_unimplemented";return Capture::Rejected;}
        if(!at(m,receiver,0x6c0,flags,e))return Capture::Rejected;
        if(flags[0]==1||flags[1]==0)return Capture::Forward;
        Intent a,b;if(!read(a)||!read(b)){if(e.empty())e="scenery_capture_children_unavailable";return Capture::Rejected;}
        if(a!=b){e="scenery_capture_changed_during_read";return Capture::Rejected;}
        out=a.state;return Capture::Single;
    }catch(const std::exception& x){e=std::string("scenery_capture_exception: ")+x.what();return Capture::Rejected;}
    catch(...){e="scenery_capture_nonstandard_exception";return Capture::Rejected;}
}
}
