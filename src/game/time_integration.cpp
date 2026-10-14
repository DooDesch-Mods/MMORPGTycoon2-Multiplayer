#include "time_integration.h"
#include <mutex>
#include <stdexcept>
#include <string_view>
#ifndef MT2MP_TIME_INTEGRATION_TEST
#include "mt2mp/core/config.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/net/local_session.h"
#include "mt2mp/net/session_wire.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/net/sidecar_client.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace mt2mp::game::time_integration {
static_assert(channel==wire::channel && packet_size==wire::wire_size);
namespace {
wire::Report rejected(wire::Code code,const std::string& cause){wire::Report r;r.code=code;r.cause=cause;return r;}
}
struct Adapter::Impl {
    runtime::Runtime engine;
    SidecarServices io;
    ui::Config config;
    Status state;
    std::recursive_mutex mutex;
    bool busy=false;
    struct Operation {
        Impl& owner;bool entered=false;
        Operation(Impl& x,const char* op,std::string& error):owner(x){
            if(x.busy){++x.state.reentrant_rejected;x.state.last_reentrant_operation=op;error="time_integration_reentrant";return;}
            x.busy=true;entered=true;
        }
        ~Operation(){if(entered)owner.busy=false;}
        explicit operator bool()const{return entered;}
    };
    struct Link {Adapter* owner=nullptr;};
    std::shared_ptr<Link> link=std::make_shared<Link>();
    void event(const char* op,const std::string& cause,const char* level="error") {
        if(state.last_operation==op&&state.last_error==cause){++state.error_repeats;return;}
        state.last_operation=op;state.last_error=cause;state.error_repeats=1;
        if(io.log){
            try{io.log(level,op,cause,1);}
            catch(const std::exception& e){state.last_error=cause+"; logger_exception: "+e.what();}
            catch(...){state.last_error=cause+"; logger_exception: non-standard exception";}
        }
    }
    bool fail(const char* op,const std::string& cause,std::string& error){error=cause;event(op,cause);return false;}
    void recovered(const char* op){
        if(state.last_operation==op&&!state.last_error.empty()){
            const auto previous=state.last_error;
            if(io.log){
                try{io.log("info",op,"recovered: "+previous,state.error_repeats);}
                catch(const std::exception& e){state.last_error=previous+"; logger_exception: "+e.what();return;}
                catch(...){state.last_error=previous+"; logger_exception: non-standard exception";return;}
            }
            state.last_operation.clear();state.last_error.clear();state.error_repeats=0;
        }
    }
    bool thread(const char* op,std::string& error){
        if(!state.initialized)return fail(op,"time_integration_not_initialized",error);
        if(io.thread_id()!=config.game_thread)return fail(op,"time_integration_wrong_game_thread",error);
        return true;
    }
    static bool session(void* p,runtime::Session& out,std::string& error){
        auto& self=*static_cast<Impl*>(p);SessionInputs input{};
        if(!self.io.read_session(input,error))return false;
        if(input.local_peer<1||input.local_peer>4){error="configured_time_peer_out_of_range actual="+std::to_string(input.local_peer);return false;}
        const auto peer=static_cast<uint8_t>(input.local_peer);
        if(input.host!=(peer==1)){error="configured_time_role_peer_mismatch";return false;}
        if(!input.session_connected||!input.transport_connected){error="time_session_or_transport_disconnected";return false;}
        if(input.peer_mask>15||!(input.peer_mask&1u)||!(input.peer_mask&(1u<<(peer-1)))){
            error="authenticated_time_peer_mask_invalid actual="+std::to_string(input.peer_mask);return false;
        }
        out={input.epoch,input.host?wire::Role::Host:wire::Role::Client,peer,input.peer_mask,
            input.session_connected&&input.transport_connected&&input.epoch!=0};
        return true;
    }
    static runtime::SendResult send(void* p,uint8_t target,const std::array<uint8_t,64>& packet,std::string& error){
        auto& self=*static_cast<Impl*>(p);
        if(self.io.send(channel,packet.data(),uint32_t(packet.size()),target))return runtime::SendResult::Accepted;
        error=self.io.failure_reason();
        if(error.empty())error="sidecar_send_returned_false_without_failure_reason";
        return runtime::SendResult::Failed;
    }
    static void log(void* p,const char* level,const char* op,const std::string& cause,uint64_t repeats){
        auto& self=*static_cast<Impl*>(p);if(self.io.log)self.io.log(level,op,cause,repeats);
    }
    // Invalid membership must retire before Runtime.ensure can reject session parsing.
    bool prepare(std::string& error){
        runtime::Session current{};bool valid=false;
        try{valid=session(this,current,error);}catch(const std::exception& e){error=e.what();}
        catch(...){error="non-standard time session callback exception";}
        if(valid&&current.snapshot_active)return true;
        const auto cause=error.empty()?"time_snapshot_inactive_or_disconnected":error;
        runtime::Status status{};std::string detail;
        if(!engine.status(status,detail)){error=detail;return false;}
        if(status.phase==runtime::Phase::Active||status.phase==runtime::Phase::ReadOnly){
            ++state.forced_suspends;
            if(!engine.suspend(cause,detail)){error=cause+"; suspend: "+detail;return false;}
        }
        if(!valid){error=cause;return false;}
        return true;
    }
};
Adapter::Adapter():impl_(std::make_unique<Impl>()){impl_->link->owner=this;}
Adapter::~Adapter(){impl_->link->owner=nullptr;}
bool Adapter::initialize(const ui::Config& c,const SidecarServices& s,std::string& e){return initialize_impl(c,s,nullptr,e);}
bool Adapter::initialize_with_backend(const ui::Config& c,const SidecarServices& s,const runtime::Native& n,std::string& e){return initialize_impl(c,s,&n,e);}
bool Adapter::initialize_impl(const ui::Config& c,const SidecarServices& io,const runtime::Native* native,std::string& error){
    auto& x=*impl_;std::lock_guard lock(x.mutex);
    Impl::Operation operation(x,"initialize",error);if(!operation)return false;
    if(x.state.initialization_attempted){error="time_integration_initialization_already_attempted";return false;}
    x.state.initialization_attempted=true;
    try{
        x.io=io;x.config=c;
        if(!io.thread_id||!io.read_session||!io.send||!io.failure_reason||!io.register_handler||!io.log)
            return x.fail("initialize","time_sidecar_callbacks_missing",error);
        if(!c.game_thread||io.thread_id()!=c.game_thread)return x.fail("initialize","time_initialize_requires_bound_game_thread",error);
        runtime::Services services{&x,Impl::session,Impl::send,Impl::log};
        const bool ready=native?x.engine.initialize_with_backend(c,services,*native,error):x.engine.initialize(c,services,error);
        if(!ready)return x.fail("initialize",error,error);
        x.state.registration_attempted=true;
        std::weak_ptr<Impl::Link> weak=x.link;
        io.register_handler(channel,[weak](uint64_t sender,const uint8_t* bytes,uint32_t size){
            const auto link=weak.lock();
            if(!link||!link->owner)throw std::runtime_error("time_channel_callback_owner_expired");
            // receive owns error reporting; Sidecar dispatch must not discard its result silently.
            const auto result=link->owner->receive(sender,bytes,size);
            if(!result.okay())return;
        });
        x.state.handler_registered=true;x.state.initialized=true;error.clear();return true;
    }catch(const std::exception& e){return x.fail("initialize",e.what(),error);}
    catch(...){return x.fail("initialize","non-standard time initialization exception",error);}
}
bool Adapter::set_hook_mask(uint32_t mask,std::string& e){
    auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Operation operation(x,"hook_mask",e);if(!operation)return false;
    try{if(!x.thread("hook_mask",e))return false;if(!x.engine.set_hook_mask(mask,e))return x.fail("hook_mask",e,e);return true;}
    catch(const std::exception& exception){return x.fail("hook_mask",exception.what(),e);}
    catch(...){return x.fail("hook_mask","non-standard hook mask exception",e);}
}
bool Adapter::tick(std::string& error){
    auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Operation operation(x,"tick",error);if(!operation)return false;
    try{
        if(!x.thread("tick",error))return false;++x.state.tick_attempted;
        std::string preflight;const bool prepared=x.prepare(preflight);
        std::string detail;const bool ticked=x.engine.tick(detail);
        if(!prepared||!ticked){error=prepared?detail:preflight;if(!prepared&&!detail.empty()&&detail!=preflight)error+="; runtime: "+detail;
            const bool waiting=error=="snapshot_not_active"||error=="time_hooks_not_ready";
            x.event("tick",error,waiting?"info":"error");return false;}
        error.clear();x.recovered("tick");return true;
    }catch(const std::exception& e){return x.fail("tick",e.what(),error);}
    catch(...){return x.fail("tick","non-standard time tick exception",error);}
}
wire::Report Adapter::receive(uint64_t sender,const uint8_t* bytes,size_t size){
    auto& x=*impl_;std::lock_guard lock(x.mutex);std::string error;
    Impl::Operation operation(x,"receive",error);if(!operation)return rejected(wire::Code::Reentrant,error);
    ++x.state.receive_attempted;
    auto failure=[&](wire::Code code,const std::string& cause){++x.state.receive_failed;x.event("receive",cause);return rejected(code,cause);};
    try{
        if(!x.thread("receive",error))return failure(wire::Code::WrongThread,error);
        if(sender<1||sender>4)return failure(wire::Code::UntrustedSender,"authenticated_time_sender_out_of_range actual="+std::to_string(sender));
        if(!bytes||size!=packet_size)return failure(wire::Code::Malformed,"time_packet_size actual="+std::to_string(size)+" required=64");
        if(!x.prepare(error))return failure(wire::Code::UntrustedSender,error);
        auto result=x.engine.receive(static_cast<uint8_t>(sender),bytes,size);
        if(!result.okay()){++x.state.receive_failed;x.event("receive",result.cause);return result;}
        ++x.state.receive_accepted;x.recovered("receive");return result;
    }catch(const std::exception& e){return failure(wire::Code::CallbackException,e.what());}
    catch(...){return failure(wire::Code::CallbackException,"non-standard time receive exception");}
}
wire::Report Adapter::submit(wire::Intent intent){
    auto& x=*impl_;std::lock_guard lock(x.mutex);std::string e;Impl::Operation operation(x,"submit",e);if(!operation)return rejected(wire::Code::Reentrant,e);
    try{if(!x.thread("submit",e))return rejected(wire::Code::WrongThread,e);if(!x.prepare(e)){x.event("submit",e);return rejected(wire::Code::NotStarted,e);}auto r=x.engine.submit(intent);if(!r.okay())x.event("submit",r.cause);return r;}
    catch(const std::exception& exception){x.event("submit",exception.what());return rejected(wire::Code::CallbackException,exception.what());}
    catch(...){e="non-standard submit exception";x.event("submit",e);return rejected(wire::Code::CallbackException,e);}
}
bool Adapter::read_presentation(ui::Presentation& out,std::string& e){
    auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Operation operation(x,"read_presentation",e);if(!operation)return false;
    try{if(!x.thread("read_presentation",e))return false;if(!x.prepare(e)||!x.engine.read_presentation(out,e))return x.fail("read_presentation",e,e);x.recovered("read_presentation");return true;}
    catch(const std::exception& exception){return x.fail("read_presentation",exception.what(),e);}
    catch(...){return x.fail("read_presentation","non-standard presentation exception",e);}
}
bool Adapter::observe_receipt(uint64_t peer,uint64_t sequence,wire::Message& out,bool& available,std::string& e){
    auto& x=*impl_;std::lock_guard lock(x.mutex);available=false;Impl::Operation operation(x,"observe_receipt",e);if(!operation)return false;
    try{if(!x.thread("observe_receipt",e))return false;if(peer<1||peer>4)return x.fail("observe_receipt","time_receipt_peer_out_of_range",e);return x.engine.observe_receipt(static_cast<uint8_t>(peer),sequence,out,available,e);}
    catch(const std::exception& exception){return x.fail("observe_receipt",exception.what(),e);}
    catch(...){return x.fail("observe_receipt","non-standard receipt exception",e);}
}
// Runtime retains receiver classification for nested native hooks. Service callbacks do not dispatch UI.
ui::HookResult Adapter::on_activate(void* type,void* item,uintptr_t caller){auto& x=*impl_;std::lock_guard lock(x.mutex);std::string e;Impl::Operation operation(x,"activate",e);if(!x.state.initialized){ui::HookResult r;r.error="time_hook_before_adapter_initialized";if(operation)x.event("activate",r.error);return r;}return x.engine.on_activate(type,item,caller);}
ui::HookResult Adapter::on_update(void* type,void* item){auto& x=*impl_;std::lock_guard lock(x.mutex);std::string e;Impl::Operation operation(x,"update",e);if(!x.state.initialized){ui::HookResult r;r.error="time_hook_before_adapter_initialized";if(operation)x.event("update",r.error);return r;}return x.engine.on_update(type,item);}
bool Adapter::suspend(const std::string& reason,std::string& e){
    auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Operation operation(x,"suspend",e);if(!operation)return false;
    try{if(!x.thread("suspend",e))return false;if(!x.engine.suspend(reason,e))return x.fail("suspend",e,e);return true;}
    catch(const std::exception& exception){return x.fail("suspend",exception.what(),e);}
    catch(...){return x.fail("suspend","non-standard suspend exception",e);}
}
bool Adapter::status(Status& out,std::string& e){
    auto& x=*impl_;std::lock_guard lock(x.mutex);Impl::Operation operation(x,"status",e);out=x.state;if(!operation)return false;
    if(!x.state.initialized){e="time_integration_not_initialized";return false;}
    try{if(!x.thread("status",e)){out=x.state;return false;}return x.engine.status(out.runtime,e);}
    catch(const std::exception& exception){return x.fail("status",exception.what(),e);}
    catch(...){return x.fail("status","non-standard status exception",e);}
}
Adapter& instance(){static Adapter adapter;return adapter;}
#ifndef MT2MP_TIME_INTEGRATION_TEST
bool initialize(const ui::Config& c,std::string& error){
    SidecarServices io;
    io.thread_id=[](){return GetCurrentThreadId();};
    io.read_session=[](SessionInputs& s,std::string&){const auto& cfg=Config::instance();s={cfg.is_host,net::session_wire::connected(),net::session_wire::connected(),cfg.peer_id,net::current_world_epoch(),net::session_wire::peer_mask()};return true;};
    io.send=[](uint8_t ch,const uint8_t* bytes,uint32_t size,uint64_t target){return net::session_wire::send(ch,bytes,size,target);};
    io.failure_reason=[](){return net::session_wire::failure_reason();};
    io.register_handler=[](uint8_t ch,Handler handler){net::session_wire::register_handler(ch,std::move(handler));};
    io.log=[](const char* level,const char* op,const std::string& cause,uint64_t repeats){
        if(std::string_view(level)=="error")LOG_ERROR("[TimeIntegration] operation={} cause={} repeats={}",op,cause,repeats);
        else if(std::string_view(level)=="warn")LOG_WARN("[TimeIntegration] operation={} cause={} repeats={}",op,cause,repeats);
        else LOG_INFO("[TimeIntegration] operation={} cause={} repeats={}",op,cause,repeats);
    };
    return instance().initialize(c,io,error);
}
#endif
}
