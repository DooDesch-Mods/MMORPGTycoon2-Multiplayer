#include "selection_witness.h"
#include <algorithm>
#include <limits>

namespace mt2mp::build_actions::private_native {
namespace {
constexpr uintptr_t selection_slot=0x13df630,selection_ap=0x17205d0,array_ap=0x1748c50;
bool fail(std::string& e,const char* cause){e=cause;return false;}
template<class T> bool at(Memory& m,uintptr_t p,size_t off,T& v,std::string& e){
    if(p>std::numeric_limits<uintptr_t>::max()-off)return fail(e,"selection_address_overflow");
    if(m.copy(p+off,&v,sizeof(v),e))return true;
    if(e.empty())e="selection_memory_read_failed";
    return false;
}
bool array(Memory& m,uintptr_t image,uintptr_t p,size_t off,std::vector<uintptr_t>& out,std::string& e){
    uintptr_t ap=0,data=0;int32_t count=0,capacity=0;
    if(!at(m,p,off,ap,e)||!at(m,p,off+8,data,e)||!at(m,p,off+16,count,e)||!at(m,p,off+20,capacity,e))return false;
    if(ap!=image+array_ap)return fail(e,"selection_array_addresspoint");
    if(count<0||capacity<count)return fail(e,"selection_array_count_capacity");
    if(count==0){out.clear();return true;}
    if(!data)return fail(e,"selection_array_data_null");
    const auto bytes=static_cast<size_t>(count)*sizeof(uintptr_t);
    if(bytes>std::numeric_limits<uintptr_t>::max()-data)return fail(e,"selection_array_range_overflow");
    out.resize(static_cast<size_t>(count));
    if(!m.copy(data,out.data(),bytes,e)){if(e.empty())e="selection_array_memory_read_failed";return false;}
    if(std::find(out.begin(),out.end(),uintptr_t{0})!=out.end())return fail(e,"selection_array_null_member");
    return true;
}
std::vector<uintptr_t> minus(const std::vector<uintptr_t>& list,uintptr_t target){
    auto result=list;result.erase(std::remove(result.begin(),result.end(),target),result.end());return result;
}
bool stable(const SelectionWitness& a,const SelectionWitness& b,std::string& e){
    if(a.receiver!=b.receiver)return fail(e,"selection_receiver_changed");
    if(a.info_window!=b.info_window)return fail(e,"selection_info_window_changed");
    if(a.filters!=b.filters)return fail(e,"selection_filters_changed");
    if(a.queue_mode!=b.queue_mode)return fail(e,"selection_queue_mode_changed");
    if(b.selection_in_progress)return fail(e,"selection_reentrant");
    if(a.hide_selected_outline!=b.hide_selected_outline)return fail(e,"selection_outline_mode_changed");
    return true;
}
}
bool read_selection(Memory& m,uintptr_t image,uintptr_t receiver,SelectionWitness& out,SelectionCounts& counts,std::string& error){
    counts={};out={};error.clear();
    try{
        if(!image||!receiver)return fail(error,"selection_receiver_missing");
        if(image>std::numeric_limits<uintptr_t>::max()-selection_slot)return fail(error,"selection_image_overflow");
        SelectionWitness first,second;
        auto once=[&](SelectionWitness& s)->bool{
            uintptr_t singleton=0,ap=0;
            if(!at(m,image,selection_slot,singleton,error)||!at(m,receiver,0,ap,error))return false;
            if(singleton!=receiver)return fail(error,"selection_singleton_mismatch");
            if(ap!=image+selection_ap)return fail(error,"selection_addresspoint_mismatch");
            s.receiver=receiver;
            constexpr size_t offsets[]{0x8,0x20,0x38,0x50};
            for(size_t i=0;i<s.lists.size();++i)if(!array(m,image,receiver,offsets[i],s.lists[i],error))return false;
            if(!at(m,receiver,0x80,s.focused,error)||!at(m,receiver,0xc0,s.info_window,error)||
               !at(m,receiver,0xc8,s.filters,error)||!at(m,receiver,0xd1,s.selection_in_progress,error)||
               !at(m,receiver,0xd2,s.hide_selected_outline,error)||!at(m,receiver,0xd3,s.queue_mode,error))return false;
            for(auto flag:s.filters)if(flag>1)return fail(error,"selection_filter_not_bool");
            if(s.selection_in_progress>1||s.hide_selected_outline>1||s.queue_mode>1)return fail(error,"selection_state_not_bool");
            return true;
        };
        if(!once(first)||!once(second))return false;
        if(first!=second)return fail(error,"selection_changed_during_read");
        out=std::move(second);counts={out.lists[0].size(),out.lists[1].size(),out.lists[2].size(),out.lists[3].size(),true};return true;
    }catch(const std::exception& ex){error=std::string("selection_read_exception: ")+ex.what();return false;}
    catch(...){return fail(error,"selection_read_unknown_exception");}
}
bool validate_selective_removal(const SelectionWitness& a,const SelectionWitness& b,uintptr_t target,std::string& e){
    if(!target)return fail(e,"selection_target_missing");
    if(!stable(a,b,e))return false;
    if(a.queue_mode)return fail(e,"selection_queue_mode_effect_unclassified");
    if(b.lists[0]!=a.lists[0])return fail(e,"selection_queued_members_changed");
    if(b.lists[1]!=minus(a.lists[1],target))return fail(e,"selection_raw_survivors_changed");
    if(b.lists[2]!=minus(a.lists[2],target))return fail(e,"selection_filtered_survivors_changed");
    const bool selected=std::find(a.lists[1].begin(),a.lists[1].end(),target)!=a.lists[1].end();
    if(selected&&!b.lists[3].empty())return fail(e,"selection_native_highlight_clear_missing");
    if(!selected&&b.lists[3]!=a.lists[3])return fail(e,"selection_unselected_highlights_changed");
    if(b.focused==target)return fail(e,"selection_target_still_focused");
    return true;
}
bool validate_restored_selection(const SelectionWitness& a,const SelectionWitness& b,uintptr_t target,std::string& e){
    if(!stable(a,b,e))return false;
    if(b.lists[0]!=minus(a.lists[0],target)||b.lists[1]!=minus(a.lists[1],target)||b.lists[2]!=minus(a.lists[2],target))return fail(e,"selection_retirement_members_changed");
    if(b.lists[3]!=minus(a.lists[3],target))return fail(e,"selection_highlight_restore_mismatch");
    if(b.focused==target)return fail(e,"selection_retired_target_focused");
    return true;
}
std::vector<uintptr_t> surviving_highlights(const SelectionWitness& a,uintptr_t target){return minus(a.lists[3],target);}
}
