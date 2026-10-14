#include "hud_calendar.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <stdexcept>
namespace mt2mp::game::hud_time {
namespace {
struct Busy {bool& v;explicit Busy(bool& value):v(value){v=true;}~Busy(){v=false;}};
bool fail(std::string& e,const char* reason){e=reason;return false;}
bool fields_valid(const CalendarFields& f){return f.seconds>=0&&f.seconds<=86400&&f.calendar_format==(f.software_calendar_enabled&&f.day>=0);}
struct alignas(16) ScalarClock {
    std::array<unsigned char,0x50> bytes{};
    explicit ScalarClock(const CalendarFields& f){
        const int32_t values[]{f.year,f.month,f.day,f.flat_day,f.seconds};
        std::memcpy(bytes.data()+0x38,values,sizeof(values));
    }
};
}
bool Calendar::initialize(const Config& config,const Services& reads,const CalendarServices& io,std::string& e){
    std::lock_guard lock(mutex_);if(busy_)return fail(e,"calendar_reentrant");Busy busy(busy_);
    if(attempted_)return fail(e,"calendar_initialize_already_attempted");attempted_=true;
    try{
        if(!io.context||!io.call_panel)return fail(e,"calendar_callback_missing");io_=io;
        if(!reader_.initialize(config,reads,e))return false;
        struct Guard {uintptr_t rva;const char* bytes;size_t count;};
        constexpr Guard guards[]{
            {0x25a190,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\x28\x05\x00\x00",19},
            {0x62c433,"\xe8\x58\xdd\xc2\xff",5},
            {0x62c630,"\x48\x83\xe9\x18\xe9\xc7\xfd\xff\xff",9}};
        for(const auto& g:guards){std::array<char,19> actual{};if(!reader_.bytes(config.image+g.rva,actual.data(),g.count,e))return false;
            if(std::memcmp(actual.data(),g.bytes,g.count))return fail(e,"calendar_native_prologue_or_caller_mismatch");}
        status_.initialized=true;e.clear();return true;
    }catch(const std::exception& ex){e=std::string("calendar_initialize_exception: ")+ex.what();return false;}
     catch(...){return fail(e,"calendar_initialize_unknown_exception");}
}
void Calendar::retire(){if(status_.bound)retired_epoch_=context_.epoch;status_.bound=false;status_.has_sample=false;binding_={};context_={};sample_={};bound_text_pane_=0;}
bool Calendar::set_hook_mask(uint32_t mask,std::string& e){
    std::lock_guard lock(mutex_);if(busy_||scope_depth_)return fail(e,"calendar_busy");Busy busy(busy_);
    try{if(!status_.initialized)return fail(e,"calendar_not_initialized");if(reader_.io_.thread_id(reader_.io_.user)!=reader_.config_.game_thread)return fail(e,"calendar_wrong_thread");
        if(mask&~3u)return fail(e,"calendar_hook_mask_invalid");status_.hook_mask=mask;if(mask!=3){retire();status_.error="calendar_hook_coverage_lost";}e.clear();return true;
    }catch(const std::exception& ex){e=std::string("calendar_mask_exception: ")+ex.what();return false;}
     catch(...){return fail(e,"calendar_mask_unknown_exception");}
}
bool Calendar::inspect(CalendarContext& context,CalendarFields& fields,Reader::Witness& witness,std::string& e){
    if(!status_.initialized)return fail(e,"calendar_not_initialized");
    if(reader_.io_.thread_id(reader_.io_.user)!=reader_.config_.game_thread)return fail(e,"calendar_wrong_thread");
    if(status_.locked)return fail(e,"calendar_locked_unknown_result");
    if(!io_.context(io_.user,context,e)){e="calendar_context_failed: "+e;return false;}
    if(!context.active||!context.epoch||!context.time_revision){retire();return fail(e,"calendar_context_not_active");}
    if(context.epoch==retired_epoch_)return fail(e,"calendar_epoch_retired");
    if(status_.bound&&(context.epoch!=context_.epoch||context.client!=context_.client)){retire();return fail(e,"calendar_context_changed");}
    if(status_.bound&&context.time_revision<context_.time_revision)return fail(e,"calendar_time_revision_stale");
    State state{};
    if(!reader_.read(state,e))return false;state={};if(!reader_.once(state,witness,e))return false;
    if(status_.bound&&!std::equal(binding_.roots.begin(),binding_.roots.begin()+7,witness.roots.begin())){retire();return fail(e,"calendar_world_or_panel_changed");}
    if(context.client&&state.native_selected_scale!=0)return fail(e,"calendar_client_clock_not_zero");
    uintptr_t text_pane=0,parent=0,owner=0;
    if(!reader_.at(witness.roots[4],0x4e8,text_pane,e)||!reader_.vtable(text_pane,0x16dc680,"calendar_text_pane",e)||
       !reader_.at(text_pane,0x278,parent,e)||!reader_.at(text_pane,0x280,owner,e))return false;
    if(parent!=witness.roots[4]||owner!=parent)return fail(e,"calendar_text_pane_owner_mismatch");
    int matches=0;
    for(size_t offset:{size_t(0x2c8),size_t(0x2f8)}){uintptr_t data=0;int32_t count=0;
        if(!reader_.at(parent,offset+8,data,e)||!reader_.at(parent,offset+0x10,count,e))return false;
        if(count<0||count>256)return fail(e,"calendar_text_membership_bound");
        for(int32_t i=0;i<count;++i){uintptr_t child=0;if(!reader_.at(data,size_t(i)*8,child,e))return false;if(child==text_pane)++matches;}}
    if(matches!=1)return fail(e,"calendar_text_pane_not_unique_member");
    if(status_.bound&&text_pane!=bound_text_pane_){retire();return fail(e,"calendar_text_pane_replaced");}
    uintptr_t software=0;int32_t software_major=0;
    if(!reader_.at(reader_.config_.image,0x13df500,software,e)||!reader_.vtable(software,0x1715450,"calendar_software",e)||!reader_.at(software,0x88,software_major,e))return false;
    const uintptr_t clock=witness.roots[5];
    if(!reader_.at(clock,0x38,fields.year,e)||!reader_.at(clock,0x3c,fields.month,e)||!reader_.at(clock,0x40,fields.day,e)||
       !reader_.at(clock,0x44,fields.flat_day,e)||!reader_.at(clock,0x48,fields.seconds,e))return false;
    fields.software_calendar_enabled=software_major>0;fields.calendar_format=fields.software_calendar_enabled&&fields.day>=0;
    if(!fields_valid(fields))return fail(e,"calendar_native_fields_invalid");
    CalendarContext after{};if(!io_.context(io_.user,after,e)){e="calendar_context_recheck_failed: "+e;return false;}
    if(context!=after)return fail(e,"calendar_context_changed_during_read");
    uintptr_t after_software=0;int32_t after_major=0;CalendarFields again{};
    if(!reader_.at(reader_.config_.image,0x13df500,after_software,e)||after_software!=software)return fail(e,"calendar_software_changed_during_read");
    if(!reader_.at(after_software,0x88,after_major,e)||!reader_.at(clock,0x38,again.year,e)||!reader_.at(clock,0x3c,again.month,e)||
       !reader_.at(clock,0x40,again.day,e)||!reader_.at(clock,0x44,again.flat_day,e)||!reader_.at(clock,0x48,again.seconds,e))return false;
    again.software_calendar_enabled=after_major>0;again.calendar_format=again.software_calendar_enabled&&again.day>=0;
    if(fields!=again)return fail(e,"calendar_fields_changed_during_read");
    inspected_text_pane_=text_pane;
    return true;
}
bool Calendar::bind(std::string& e){
    std::lock_guard lock(mutex_);if(busy_||scope_depth_)return fail(e,"calendar_busy");Busy busy(busy_);
    try{if(status_.bound)return fail(e,"calendar_already_bound");if(status_.hook_mask!=3)return fail(e,"calendar_hooks_not_ready");
        CalendarContext context{};CalendarFields fields{};Reader::Witness witness{};if(!inspect(context,fields,witness,e)){status_.error=e;return false;}
        context_=context;binding_=witness;bound_text_pane_=inspected_text_pane_;status_.bound=true;status_.epoch=context.epoch;status_.revision=0;status_.has_sample=false;status_.error.clear();e.clear();return true;
    }catch(const std::exception& ex){e=std::string("calendar_bind_exception: ")+ex.what();status_.error=e;return false;}
     catch(...){status_.error="calendar_bind_unknown_exception";return fail(e,"calendar_bind_unknown_exception");}
}
bool Calendar::suspend(const std::string& reason,std::string& e){
    std::lock_guard lock(mutex_);if(busy_||scope_depth_)return fail(e,"calendar_busy");retire();status_.error=reason;e.clear();return true;
}
bool Calendar::read_host_probe(CalendarSample& out,std::string& e){
    std::lock_guard lock(mutex_);if(busy_||scope_depth_)return fail(e,"calendar_busy");Busy busy(busy_);out={};
    try{if(!status_.bound)return fail(e,"calendar_not_bound");CalendarContext context{};CalendarFields fields{};Reader::Witness witness{};
        if(!inspect(context,fields,witness,e))return false;if(context.client)return fail(e,"calendar_probe_requires_host");
        uint64_t revision=sample_.revision;
        if(!status_.has_sample||fields!=sample_.fields||context.time_revision!=sample_.time_revision){
            if(revision==UINT64_MAX)return fail(e,"calendar_revision_exhausted");++revision;}
        sample_={context.epoch,revision,context.time_revision,fields};status_.has_sample=true;status_.revision=revision;context_=context;out=sample_;++status_.probes;status_.error.clear();e.clear();return true;
    }catch(const std::exception& ex){e=std::string("calendar_probe_exception: ")+ex.what();status_.error=e;return false;}
     catch(...){status_.error="calendar_probe_unknown_exception";return fail(e,"calendar_probe_unknown_exception");}
}
bool Calendar::receive(uint64_t sender,const CalendarSample& sample,std::string& e){
    std::lock_guard lock(mutex_);if(busy_||scope_depth_)return fail(e,"calendar_busy");Busy busy(busy_);
    try{++status_.received;
        if(sender!=1)return fail(e,"calendar_untrusted_sender");if(!status_.bound)return fail(e,"calendar_not_bound");
        CalendarContext context{};CalendarFields local{};Reader::Witness witness{};if(!inspect(context,local,witness,e))return false;
        if(!context.client)return fail(e,"calendar_receive_requires_client");
        if(sample.epoch!=context.epoch||!sample.revision||!sample.time_revision||sample.time_revision>context.time_revision||!fields_valid(sample.fields))return fail(e,"calendar_sample_domain_or_epoch_invalid");
        if(status_.has_sample&&sample.revision<sample_.revision)return fail(e,"calendar_sample_revision_stale");
        if(status_.has_sample&&sample.time_revision<sample_.time_revision)return fail(e,"calendar_sample_time_revision_regressed");
        if(status_.has_sample&&sample.revision==sample_.revision){if(sample!=sample_)return fail(e,"calendar_sample_revision_conflict");++status_.duplicate;e.clear();return true;}
        if((local.software_calendar_enabled&&sample.fields.day>=0)!=sample.fields.calendar_format)return fail(e,"calendar_format_gate_mismatch");
        sample_=sample;status_.has_sample=true;status_.revision=sample.revision;context_=context;status_.error.clear();e.clear();return true;
    }catch(const std::exception& ex){e=std::string("calendar_receive_exception: ")+ex.what();status_.error=e;return false;}
     catch(...){status_.error="calendar_receive_unknown_exception";return fail(e,"calendar_receive_unknown_exception");}
}
bool Calendar::read_sample(CalendarSample& out,std::string& e) const {
    std::lock_guard lock(mutex_);out={};
    if(!status_.bound||!status_.has_sample||status_.locked)return fail(e,"calendar_sample_unavailable");
    out=sample_;e.clear();return true;
}
bool Calendar::scope_ready(CalendarContext& context,CalendarFields& local,Reader::Witness& witness,std::string& e){
    if(!status_.bound||status_.hook_mask!=3||!status_.has_sample)return fail(e,"calendar_presentation_waiting");
    if(!inspect(context,local,witness,e))return false;
    if(!context.client)return fail(e,"calendar_host_original");
    if(sample_.epoch!=context.epoch||sample_.time_revision>context.time_revision)return fail(e,"calendar_sample_context_stale");
    if((local.software_calendar_enabled&&sample_.fields.day>=0)!=sample_.fields.calendar_format)return fail(e,"calendar_format_gate_mismatch");
    return true;
}
bool Calendar::panel_update(void* receiver,float dt,OriginalPanelUpdate original,std::string& e){
    uintptr_t previous=0;bool track=false,owns_scope=false;
    {
        std::lock_guard lock(mutex_);e.clear();if(!original||!io_.call_panel)return fail(e,"calendar_original_panel_missing");
        ++status_.panel_original_attempted;
        try{if(reader_.io_.thread_id&&reader_.io_.thread_id(reader_.io_.user)==reader_.config_.game_thread){previous=panel_scope_;panel_scope_=0;++scope_depth_;owns_scope=true;}
            else e="calendar_wrong_thread";}
        catch(const std::exception& ex){e=std::string("calendar_thread_guard_exception: ")+ex.what();}
        catch(...){e="calendar_thread_guard_unknown_exception";}
        if(owns_scope&&!busy_&&scope_depth_==1&&status_.bound&&context_.client){Busy busy(busy_);
            try{CalendarContext context{};CalendarFields fields{};Reader::Witness witness{};
                if(scope_ready(context,fields,witness,e)&&reinterpret_cast<uintptr_t>(receiver)==witness.roots[4]){panel_scope_=witness.roots[4];track=true;}
                else if(e.empty())e="calendar_foreign_panel";
            }catch(const std::exception& ex){e=std::string("calendar_panel_guard_exception: ")+ex.what();}
             catch(...){e="calendar_panel_guard_unknown_exception";}}
    }
    bool original_ok=false;std::string original_error;
    try{original_ok=io_.call_panel(io_.user,original,receiver,dt,original_error);}
    catch(const std::exception& ex){original_error=std::string("calendar_original_panel_exception: ")+ex.what();}
    catch(...){original_error="calendar_original_panel_unknown_exception";}
    {
        std::lock_guard lock(mutex_);if(owns_scope){panel_scope_=previous;--scope_depth_;}
        if(!original_ok){e="calendar_original_panel_failed: "+original_error;if(track||formatter_inflight_)status_.locked=true;formatter_inflight_=false;}
        if(status_.locked&&e.empty())e=status_.error;
        if(!e.empty())status_.error=e;
    }
    return original_ok&&e.empty();
}
void* Calendar::format(void* out,void* receiver,uintptr_t caller,OriginalTimeString original){
    if(!original)throw std::invalid_argument("calendar_original_formatter_missing");
    CalendarSample selected{};bool substitute=false;
    {
        std::lock_guard lock(mutex_);++status_.formatter_original_attempted;
        if(!busy_&&!formatter_inflight_&&panel_scope_&&caller==0x62c438&&out){Busy busy(busy_);
            std::string error;
            try{CalendarContext context{};CalendarFields local{};Reader::Witness witness{};
                if(scope_ready(context,local,witness,error)&&witness.roots[4]==panel_scope_&&reinterpret_cast<uintptr_t>(receiver)==witness.roots[5]){
                    selected=sample_;substitute=true;formatter_inflight_=true;++status_.formatter_substituted;
                }else if(error.empty())error="calendar_foreign_formatter_receiver";
            }catch(const std::exception& ex){error=std::string("calendar_formatter_guard_exception: ")+ex.what();}
             catch(...){error="calendar_formatter_guard_unknown_exception";}
            if(!error.empty()){status_.error=error;++status_.rejected;}
        }
    }
    ScalarClock scalar(selected.fields);void* result=nullptr;
    try{result=original(out,substitute?static_cast<void*>(scalar.bytes.data()):receiver);}
    catch(const std::exception& ex){std::lock_guard lock(mutex_);if(substitute){status_.locked=true;formatter_inflight_=false;}status_.error=std::string("calendar_original_formatter_exception: ")+ex.what();throw;}
    catch(...){std::lock_guard lock(mutex_);if(substitute){status_.locked=true;formatter_inflight_=false;}status_.error="calendar_original_formatter_unknown_exception";throw;}
    if(substitute){std::lock_guard lock(mutex_);formatter_inflight_=false;
        if(result!=out){status_.locked=true;status_.error="calendar_native_sret_return_mismatch";}else{++status_.formatter_returned;status_.formatted_epoch=selected.epoch;
            status_.formatted_revision=selected.revision;status_.formatted_time_revision=selected.time_revision;status_.formatted_fields=selected.fields;status_.error.clear();}}
    return result;
}
}
