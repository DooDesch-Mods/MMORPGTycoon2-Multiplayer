#include "hud_calendar_transport.h"
#include <bit>
#include <exception>
#include <mutex>
#include <stdexcept>
namespace mt2mp::game::hud_time::transport {
namespace {
bool fail(std::string& e,const char* s){e=s;return false;}
void put(Packet& p,size_t at,uint64_t value,size_t n){for(size_t i=0;i<n;++i)p[at+i]=uint8_t(value>>(i*8));}
uint64_t get(const uint8_t* p,size_t at,size_t n){uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(p[at+i])<<(8*i);return v;}
bool valid(const CalendarSample& s){return s.epoch&&s.revision&&s.time_revision&&s.fields.seconds>=0&&s.fields.seconds<=86400&&s.fields.calendar_format==(s.fields.software_calendar_enabled&&s.fields.day>=0);}
struct Busy {bool& value;explicit Busy(bool& v):value(v){v=true;}~Busy(){value=false;}};
bool ready(const Context& c){return c.connected&&c.active&&c.calendar_bound&&c.epoch&&c.time_revision&&c.calendar_epoch==c.epoch&&c.local_peer>=1&&c.local_peer<=4&&!(c.peer_mask&0xf0)&&
    (c.peer_mask&1)&&(c.peer_mask&(1u<<(c.local_peer-1)))&&(c.client?(c.local_peer!=1):(c.local_peer==1));}
bool binding_equal(const Context& a,const Context& b){return a.epoch==b.epoch&&a.calendar_epoch==b.calendar_epoch&&a.client==b.client&&a.local_peer==b.local_peer&&a.peer_mask==b.peer_mask;}
}
bool encode(const CalendarSample& s,Packet& p,std::string& e){
    p={};if(!valid(s))return fail(e,"calendar_wire_sample_invalid");
    put(p,0,0x314c4143,4);p[4]=1;p[5]=1;put(p,6,packet_size,2);
    put(p,8,s.epoch,8);put(p,16,s.revision,8);put(p,24,s.time_revision,8);
    const int32_t v[]{s.fields.year,s.fields.month,s.fields.day,s.fields.flat_day,s.fields.seconds};
    for(size_t i=0;i<5;++i)put(p,32+i*4,std::bit_cast<uint32_t>(v[i]),4);
    p[52]=uint8_t(s.fields.software_calendar_enabled);p[53]=uint8_t(s.fields.calendar_format);e.clear();return true;
}
bool decode(const uint8_t* p,size_t n,CalendarSample& s,std::string& e){
    s={};if(!p||n!=packet_size)return fail(e,"calendar_wire_size_invalid");
    if(get(p,0,4)!=0x314c4143)return fail(e,"calendar_wire_magic_invalid");
    if(p[4]!=1)return fail(e,"calendar_wire_version_invalid");if(p[5]!=1)return fail(e,"calendar_wire_kind_invalid");
    if(get(p,6,2)!=packet_size)return fail(e,"calendar_wire_declared_size_invalid");
    if(p[54]||p[55])return fail(e,"calendar_wire_reserved_nonzero");
    if(p[52]>1||p[53]>1)return fail(e,"calendar_wire_boolean_invalid");
    CalendarSample result{};result.epoch=get(p,8,8);result.revision=get(p,16,8);result.time_revision=get(p,24,8);
    int32_t* fields[]{&result.fields.year,&result.fields.month,&result.fields.day,&result.fields.flat_day,&result.fields.seconds};
    for(size_t i=0;i<5;++i)*fields[i]=std::bit_cast<int32_t>(uint32_t(get(p,32+i*4,4)));
    result.fields.software_calendar_enabled=p[52]!=0;result.fields.calendar_format=p[53]!=0;
    if(!valid(result))return fail(e,"calendar_wire_sample_invalid");s=result;e.clear();return true;
}
struct Adapter::Impl {
    mutable std::recursive_mutex mutex;
    Services io;Status s;Context bound{};CalendarSample inbox{},applied{},host_previous{};
    bool attempted=false,busy=false,has_inbox=false,has_applied=false,has_host=false,has_time=false;
    uint32_t game_thread=0;uint64_t last_poll=0;
    std::string first_failure;
    bool error(std::string& e,const std::string& cause){
        if(s.locked&&first_failure.empty())first_failure=cause;
        e=first_failure.empty()||first_failure==cause?cause:first_failure+"; subsequent: "+cause;
        s.error=e;++s.failures;return false;
    }
    bool cleanup_error(std::string& e,const std::string& cause){
        s.locked=true;
        first_failure=first_failure.empty()?cause:first_failure+"; "+cause;
        return error(e,first_failure);
    }
    void clear(){if(s.bound)s.retired_epoch=s.epoch;s.bound=false;s.pending=false;s.waiting.clear();has_inbox=has_applied=has_host=has_time=false;inbox={};applied={};host_previous={};bound={};}
    bool thread(std::string& e){
        if(!s.initialized)return error(e,"calendar_transport_not_initialized");
        try{if(io.thread_id()!=game_thread)return error(e,"calendar_transport_wrong_thread");return true;}
        catch(const std::exception& ex){return error(e,std::string("calendar_transport_thread_exception: ")+ex.what());}
        catch(...){return error(e,"calendar_transport_thread_unknown_exception");}
    }
    bool stop(const std::string& cause,std::string& e,bool failure=true){
        if(failure)++s.failures;
        if(s.locked&&first_failure.empty())first_failure=cause;
        clear();s.suspend_pending=true;std::string native;
        try{if(!io.suspend_calendar(cause,native))return cleanup_error(e,"calendar_suspend_failed: "+native);}
        catch(const std::exception& ex){return cleanup_error(e,std::string("calendar_suspend_exception: ")+ex.what());}
        catch(...){return cleanup_error(e,"calendar_suspend_unknown_exception");}
        s.suspend_pending=false;s.error=first_failure.empty()?cause:first_failure;e=s.error;return false;
    }
    bool current(Context& c,std::string& e){
        if(!io.context(c,e))return stop("calendar_transport_context_failed: "+e,e);
        if(!ready(c))return stop("calendar_transport_context_unavailable",e);
        if(!binding_equal(c,bound)||c.time_revision<bound.time_revision)return stop("calendar_transport_binding_changed",e);
        bound.time_revision=c.time_revision;return true;
    }
    bool enqueue(uint64_t sender,const uint8_t* bytes,size_t n,std::string& e){
        std::lock_guard lock(mutex);
        if(busy)return error(e,"calendar_transport_reentrant");
        if(sender!=1){++s.rejected;return error(e,"calendar_transport_untrusted_sender");}
        CalendarSample sample{};if(!decode(bytes,n,sample,e)){++s.rejected;const auto cause=e;return error(e,cause);}
        if(!s.bound&&!s.locked){++s.unbound_skipped;s.waiting="calendar_transport_sample_skipped_until_bind";e.clear();return true;}
        if(!bound.client||s.locked){++s.rejected;return error(e,"calendar_transport_receive_not_bound_client");}
        if(sample.epoch!=bound.epoch){++s.rejected;return error(e,"calendar_transport_sample_epoch_mismatch");}
        ++s.received;const CalendarSample* prior=has_inbox?&inbox:(has_applied?&applied:nullptr);
        if(prior){
            if(sample.revision<prior->revision){++s.rejected;return error(e,"calendar_transport_sample_stale");}
            if(sample.revision==prior->revision){if(sample!=*prior){++s.rejected;return error(e,"calendar_transport_revision_conflict");}++s.duplicates;e.clear();return true;}
            if(sample.time_revision<prior->time_revision){++s.rejected;return error(e,"calendar_transport_time_revision_regressed");}
        }
        if(has_inbox)++s.superseded;inbox=sample;has_inbox=true;s.pending=true;e.clear();return true;
    }
};
Adapter::Adapter():p_(std::make_shared<Impl>()){}
Adapter::~Adapter()=default;
bool Adapter::initialize(uint32_t thread,Services io,std::string& e){
    auto& p=*p_;std::lock_guard lock(p.mutex);if(p.busy)return p.error(e,"calendar_transport_reentrant");Busy busy(p.busy);
    if(p.attempted)return p.error(e,"calendar_transport_initialize_already_attempted");p.attempted=true;
    try{
        if(!thread||!io.thread_id||!io.context||!io.probe||!io.apply||!io.suspend_calendar||!io.send||!io.register_handler)return p.error(e,"calendar_transport_callback_missing");
        if(io.thread_id()!=thread)return p.error(e,"calendar_transport_initialize_wrong_thread");
        p.io=std::move(io);p.game_thread=thread;
        std::weak_ptr<Impl> weak=p_;p.io.register_handler(channel,[weak](uint64_t sender,const uint8_t* bytes,uint32_t size){
            auto state=weak.lock();if(!state)throw std::runtime_error("calendar_transport_handler_owner_expired");
            std::string error;if(!state->enqueue(sender,bytes,size,error))throw std::runtime_error(error);
        });p.s.initialized=true;e.clear();return true;
    }catch(const std::exception& ex){return p.error(e,std::string("calendar_transport_initialize_exception: ")+ex.what());}
    catch(...){return p.error(e,"calendar_transport_initialize_unknown_exception");}
}
bool Adapter::bind(std::string& e){
    auto& p=*p_;std::lock_guard lock(p.mutex);if(p.busy)return p.error(e,"calendar_transport_reentrant");Busy busy(p.busy);
    try{
        if(!p.thread(e))return false;if(p.s.locked)return p.error(e,"calendar_transport_locked");if(p.s.bound)return p.error(e,"calendar_transport_already_bound");
        Context c{};if(!p.io.context(c,e))return p.error(e,"calendar_transport_bind_context_failed: "+e);
        if(!ready(c))return p.error(e,"calendar_transport_bind_context_not_ready");
        if(c.epoch==p.s.retired_epoch)return p.error(e,"calendar_transport_epoch_retired");
        p.bound=c;p.s.epoch=c.epoch;p.s.bound=true;p.s.applied_revision=0;p.s.last_sent_revision=0;p.s.waiting.clear();p.s.error.clear();e.clear();return true;
    }catch(const std::exception& ex){return p.error(e,std::string("calendar_transport_bind_exception: ")+ex.what());}
    catch(...){return p.error(e,"calendar_transport_bind_unknown_exception");}
}
bool Adapter::enqueue(uint64_t sender,const uint8_t* bytes,size_t n,std::string& e){return p_->enqueue(sender,bytes,n,e);}
Status Adapter::status() const {std::lock_guard lock(p_->mutex);return p_->s;}
bool Adapter::suspend(const std::string& reason,std::string& e){
    auto& p=*p_;std::lock_guard lock(p.mutex);if(p.busy)return p.error(e,"calendar_transport_reentrant");Busy busy(p.busy);
    try{if(!p.thread(e))return false;p.stop(reason,e,false);if(p.s.suspend_pending)return false;e.clear();return true;}
    catch(const std::exception& ex){p.clear();p.s.locked=true;return p.error(e,std::string("calendar_transport_suspend_exception: ")+ex.what());}
    catch(...){p.clear();p.s.locked=true;return p.error(e,"calendar_transport_suspend_unknown_exception");}
}
bool Adapter::tick(uint64_t now,std::string& e){
    auto& p=*p_;std::lock_guard lock(p.mutex);if(p.busy)return p.error(e,"calendar_transport_reentrant");Busy busy(p.busy);
    try{
        if(!p.thread(e))return false;if(p.s.locked)return p.error(e,"calendar_transport_locked");
        if(!p.s.bound){p.s.waiting="calendar_transport_waiting_for_explicit_bind";e.clear();return true;}
        Context c{};if(!p.current(c,e))return false;
        if(c.client){
            if(!p.has_inbox){p.s.waiting="calendar_transport_waiting_for_host_sample";e.clear();return true;}
            if(p.inbox.time_revision>c.time_revision){++p.s.future_waits;p.s.waiting="calendar_transport_waiting_for_time_revision";e.clear();return true;}
            const auto selected=p.inbox;++p.s.apply_attempted;
            if(!p.io.apply(1,selected,e)){const auto cause="calendar_transport_apply_failed: "+e;p.s.locked=true;return p.stop(cause,e);}
            Context after{};if(!p.current(after,e))return false;
            p.applied=selected;p.has_applied=true;p.has_inbox=false;p.s.pending=false;++p.s.apply_confirmed;p.s.applied_revision=selected.revision;p.s.waiting.clear();e.clear();return true;
        }
        if(p.has_time&&now<p.last_poll)return p.stop("calendar_transport_monotonic_time_regressed",e);
        if(p.has_time&&now-p.last_poll<100){p.s.waiting="calendar_transport_rate_wait";e.clear();return true;}
        p.last_poll=now;p.has_time=true;CalendarSample sample{};++p.s.probed;
        if(!p.io.probe(sample,e))return p.stop("calendar_transport_probe_failed: "+e,e);
        Context after{};if(!p.current(after,e))return false;
        if(c!=after)return p.stop("calendar_transport_context_changed_during_probe",e);
        if(sample.epoch!=c.epoch||sample.time_revision!=c.time_revision||!valid(sample))return p.stop("calendar_transport_probe_domain_mismatch",e);
        if(p.has_host&&(sample.revision<p.host_previous.revision||sample.time_revision<p.host_previous.time_revision||(sample.revision==p.host_previous.revision&&sample!=p.host_previous)))return p.stop("calendar_transport_host_revision_conflict",e);
        Packet packet{};if(!encode(sample,packet,e))return p.stop("calendar_transport_encode_failed: "+e,e);
        p.host_previous=sample;p.has_host=true;
        for(uint64_t peer=2;peer<=4;++peer)if(c.peer_mask&(1u<<(peer-1))){
            ++p.s.send_attempted;if(!p.io.send(channel,packet.data(),uint32_t(packet.size()),peer,e))return p.stop("calendar_transport_send_failed peer="+std::to_string(peer)+": "+e,e);
            ++p.s.send_accepted;
        }
        p.s.last_sent_revision=sample.revision;p.s.waiting.clear();e.clear();return true;
    }catch(const std::exception& ex){p.s.locked=true;return p.stop(std::string("calendar_transport_tick_exception: ")+ex.what(),e);}
    catch(...){p.s.locked=true;return p.stop("calendar_transport_tick_unknown_exception",e);}
}
}
