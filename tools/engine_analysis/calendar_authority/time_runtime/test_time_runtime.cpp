#define main frozen_bridge_regression_main
#include "../host_time/test_client_regression.cpp"
#undef main
#include "staging/time_runtime.h"
#include <deque>
namespace rt=mt2mp::game::time_runtime;
namespace tw=mt2mp::game::time_session;
namespace mt2mp::game::time_runtime {
bool production_backend(const ui::Config&,Native&,std::string& e){e="fixture_production_backend_disabled";return false;}
}
namespace {
struct Peer {
    Fixture f;rt::Runtime runtime;rt::Session session{};std::string error;
    struct Packet {uint8_t target;std::array<uint8_t,64> bytes;};
    std::deque<Packet> sent;
    std::vector<Packet> attempts;
    rt::SendResult send_result=rt::SendResult::Accepted;
    uint64_t software_epoch=19;size_t set_calls=0,logs=0,reads=0;
    bool read_fail=false,set_false=false,set_throw=false,read_fail_after_set=false,reenter=false;
    tw::Report reentrant_report{};
    static bool ctx(void* p,rt::Session& out,std::string&){out=static_cast<Peer*>(p)->session;return true;}
    static rt::SendResult send(void* p,uint8_t target,const std::array<uint8_t,64>& bytes,std::string& e){
        auto& x=*static_cast<Peer*>(p);x.attempts.push_back({target,bytes});
        if(x.send_result==rt::SendResult::Accepted)x.sent.push_back({target,bytes});
        else e="fixture_transport_backpressure_or_error";return x.send_result;
    }
    static void log(void* p,const char*,const char*,const std::string&,uint64_t){++static_cast<Peer*>(p)->logs;}
    static bool read(void* p,tw::Clock& out,std::string& e){auto& x=*static_cast<Peer*>(p);++x.reads;
        if(x.read_fail){e="fixture_clock_read_failure";return false;}
        out={x.f.get<float>(Fixture::clock+0x58),x.f.get<float>(Fixture::clock+0x5c)};return true;}
    static bool set(void* p,float target,std::string& e){auto& x=*static_cast<Peer*>(p);++x.set_calls;
        x.f.put(Fixture::clock+0x58,target);
        if(x.reenter)x.reentrant_report=x.runtime.submit(tw::Intent::Fastest);
        if(x.set_throw)throw std::runtime_error("fixture_clock_set_exception_after_effect");
        if(x.read_fail_after_set)x.read_fail=true;
        if(x.set_false){e="fixture_clock_set_failure_after_effect";return false;}return true;}
    static bool epoch(void* p,uint64_t& out,std::string&){out=static_cast<Peer*>(p)->software_epoch;return true;}
    explicit Peer(bool host=false){
        session={19,host?tw::Role::Host:tw::Role::Client,static_cast<uint8_t>(host?1:2),3,true};
        f.put(Fixture::clock+0x5c,1.0f);
        check(runtime.initialize_with_backend(f.config(),{this,ctx,send,log},{this,f.backend(),read,set,epoch},error),"runtime_initialize");
    }
    void hooks(){check(runtime.set_hook_mask(3,error),"runtime_hooks");}
    void tick(){error.clear();check(runtime.tick(error),error.c_str());}
    rt::Status status(){rt::Status out{};check(runtime.status(out,error),"runtime_status");return out;}
    void deliver(Peer& target){
        while(!sent.empty()){auto p=sent.front();sent.pop_front();auto r=target.runtime.receive(session.local_peer,p.bytes.data(),p.bytes.size());check(r.okay(),r.cause.c_str());}
    }
    tw::Message state(uint64_t epoch_value,uint64_t revision,float scale){tw::Message m{};m.kind=tw::Kind::State;m.epoch=epoch_value;m.peer=1;m.state={true,false,revision,{scale,1}};return m;}
    tw::Report receive(const tw::Message& m,uint8_t sender=1){std::array<uint8_t,64> bytes{};check(tw::encode(m,bytes,error),"encode_fixture_packet");return runtime.receive(sender,bytes.data(),bytes.size());}
    HookResult activate(size_t index,uintptr_t caller=0x58095c){return runtime.on_activate(reinterpret_cast<void*>(Fixture::type(index)),reinterpret_cast<void*>(Fixture::item(index)),caller);}
    HookResult update(size_t index){return runtime.on_update(reinterpret_cast<void*>(Fixture::type(index)),reinterpret_cast<void*>(Fixture::item(index)));}
};
}
int main(){try{
    {
        Peer client;Presentation p{};
        check(client.runtime.read_presentation(p,client.error)&&!p.capture_enabled,"readonly_before_hooks");
        check(client.f.writes==0&&client.set_calls==0,"readonly_no_native_effect");
        check(!client.runtime.tick(client.error)&&client.error=="time_hooks_not_ready","hooks_required_before_controller");
        check(client.activate(1).decision==Decision::Suppress,"initial_client_known_activate_no_local_unpause");
        auto foreign=client.runtime.on_activate(reinterpret_cast<void*>(Fixture::type(0)),reinterpret_cast<void*>(Fixture::item(1)),0x58095c);
        check(!foreign.matched&&foreign.decision==Decision::Forward,"foreign_pair_original");
        client.hooks();client.tick();check(client.runtime.submit(tw::Intent::Normal).code==tw::Code::NoState,"no_state_no_request");
        client.f.current=9;check(!client.runtime.tick(client.error)&&client.error=="wrong_game_thread","runtime_wrong_thread");
    }
    {
        Peer host(true),client;host.hooks();client.hooks();
        client.session.snapshot_active=false;client.session.epoch=0;
        check(client.receive(client.state(19,1,3)).okay(),"early_state_retained");
        check(client.f.writes==0,"early_state_no_native_effect");
        check(client.receive(client.state(19,1,9)).code==tw::Code::ConflictingDuplicate,"early_conflict_visible");
        client.session.snapshot_active=true;client.session.epoch=19;client.tick();
        check(client.f.get<int32_t>(Fixture::item(2)+0x38)==0&&client.f.get<float>(Fixture::clock+0x58)==0,"early_state_applied_after_bind");
        check(client.status().controller.applied.revision==1,"early_state_applied_receipt");
        check(client.set_calls==0,"client_never_sets_clock");
        host.f.put(Fixture::clock+0x58,3.0f);host.tick();client.deliver(host);host.deliver(client);client.tick();client.deliver(host);
        const auto hook=client.activate(1);check(hook.request_queued&&hook.decision==Decision::Suppress,"client_native_ui_request_captured");
        check(host.set_calls==0,"activation_defers_execution");
        client.tick();check(!client.sent.empty(),"request_outbound");const auto request=client.sent.front();
        client.deliver(host);host.tick();check(host.set_calls==1&&host.f.get<float>(Fixture::clock+0x58)==1,"host_native_execute_once");
        auto replay=host.runtime.receive(2,request.bytes.data(),64);check(replay.okay()&&host.set_calls==1,"request_replay_no_native_execute");
        host.tick();host.deliver(client);client.tick();client.deliver(host);
        tw::Message receipt{};bool found=false;
        check(client.runtime.observe_receipt(2,1,receipt,found,client.error)&&found&&receipt.result==tw::Result::Confirmed,"client_immutable_receipt");
        check(client.update(2).decision==Decision::Suppress&&client.f.get<int32_t>(Fixture::item(1)+0x38)==0,"client_update_host_presentation");
        check(host.update(1).decision==Decision::Forward,"host_update_original");
        check(host.activate(2).request_queued,"host_same_ui_capture");host.tick();check(host.set_calls==2,"host_local_ui_execute_once");
        check(host.runtime.observe_receipt(1,1,receipt,found,host.error)&&found,"host_local_receipt_consumed");
        check(std::none_of(host.sent.begin(),host.sent.end(),[](const Peer::Packet& p){return p.target==1;}),"host_receipt_not_sent_to_self");
    }
    {
        Peer host(true);host.hooks();host.send_result=rt::SendResult::Backpressure;
        check(!host.runtime.tick(host.error),"initial_broadcast_backpressure_visible");
        const auto first=host.attempts.back().bytes;
        check(!host.runtime.tick(host.error)&&host.attempts.back().bytes==first,"outbound_byte_exact_backpressure_retry");
        check(host.runtime.suspend("fixture_load",host.error),"suspend_old_world");host.session.epoch=20;
        check(!host.runtime.tick(host.error)&&host.status().phase==rt::Phase::Retiring,"old_outbox_blocks_new_binding");
        host.send_result=rt::SendResult::Accepted;host.tick();
        check(host.sent.front().bytes==first&&host.status().epoch==20,"retained_packet_sent_before_new_epoch");
        host.session.epoch=19;check(!host.runtime.tick(host.error),"return_to_retired_epoch_invalidates_current");
        check(!host.runtime.tick(host.error)&&host.error=="retired_epoch_requires_new_snapshot","retired_epoch_never_rebound");
    }
    {
        Peer host(true);host.hooks();host.tick();host.set_false=true;
        auto r=host.runtime.submit(tw::Intent::Fast);check(r.code==tw::Code::NativeUnknown&&host.set_calls==1,"false_after_set_unknown");
        host.set_false=false;check(host.runtime.submit(tw::Intent::Normal).code==tw::Code::Locked&&host.set_calls==1,"unknown_host_effect_not_replayed");
        check(!host.runtime.tick(host.error)&&host.status().controller.write_locked,"host_poll_does_not_heal_unknown");
        check(host.activate(1).decision==Decision::Suppress&&!host.activate(1).request_queued,"host_locked_activate_cannot_bypass_controller");
    }
    {
        Peer host(true);host.hooks();host.tick();host.set_throw=true;
        auto r=host.runtime.submit(tw::Intent::Fast);check(r.code==tw::Code::NativeUnknown&&r.cause.find("fixture_clock_set_exception_after_effect")!=std::string::npos,"cpp_exception_cause_after_set_retained");
    }
    {
        Peer host(true);host.hooks();host.tick();host.read_fail=true;
        auto r=host.runtime.submit(tw::Intent::Fast);check(!r.okay()&&host.set_calls==0&&!host.status().controller.write_locked,"pre_read_fail_no_effect_no_unknown");
    }
    {
        Peer host(true);host.hooks();host.tick();host.read_fail_after_set=true;
        auto r=host.runtime.submit(tw::Intent::Fast);check(r.code==tw::Code::NativeUnknown&&host.status().controller.write_locked,"missing_post_read_unknown");
    }
    {
        Peer client;client.hooks();client.tick();client.f.fail_write=2;
        auto r=client.receive(client.state(19,1,3));check(r.code==tw::Code::PresentationUnknown,"partial_client_presentation_unknown");
        const auto writes=client.f.writes;check(writes==2&&client.status().item_stores_completed==1,"partial_store_counts_visible");
        check(client.activate(1).decision==Decision::Suppress&&!client.activate(1).request_queued,"partial_client_activate_fenced");
        check(client.update(1).decision==Decision::Suppress&&client.f.writes==writes,"partial_client_update_no_replay");
        r=client.receive(client.state(19,2,9));check(!r.okay()&&client.f.writes==writes,"new_revision_cannot_heal_partial");
    }
    {
        Peer client;client.hooks();client.tick();check(client.receive(client.state(19,1,3)).okay(),"client_initial_state");
        client.f.put(Fixture::item(0)+0x38,int32_t(0));client.f.lie_write=true;
        check(!client.update(0).error.empty()&&client.status().presentation_uncertain,"update_readback_failure_latched");
        auto writes=client.f.writes;check(client.update(0).decision==Decision::Suppress&&client.f.writes==writes,"update_unknown_no_repeat");
    }
    {
        Peer host(true);host.hooks();host.tick();host.reenter=true;
        check(host.runtime.submit(tw::Intent::Fast).okay()&&host.set_calls==1,"outer_execution_succeeds");
        check(host.reentrant_report.cause=="runtime_reentrant","callback_reentry_rejected");
        host.f.put(Fixture::mode+0x58,Fixture::base+0x2300000);
        check(!host.runtime.tick(host.error)&&host.error=="native_world_or_actor_changed","world_receiver_changed");
        check(!host.runtime.tick(host.error)&&host.error=="retired_epoch_requires_new_snapshot","same_epoch_new_world_rejected");
    }
    {
        Peer host(true);host.hooks();host.tick();host.f.put(Fixture::clock+0x58,2.0f);
        check(!host.runtime.tick(host.error)&&host.error=="host_scale_outside_capture_domain_original_forward","arbitrary_host_scale_visible");
        check(host.activate(1).decision==Decision::Forward&&host.set_calls==0,"arbitrary_scale_preserves_native_original");
    }
    {
        Peer client;client.session.snapshot_active=false;client.session.epoch=0;
        for(uint64_t i=1;i<=4;++i)check(client.receive(client.state(i,1,1)).okay(),"bounded_early_epoch_store");
        check(client.receive(client.state(5,1,1)).code==tw::Code::Backpressure,"early_cache_full_visible");
        check(client.receive(client.state(1,2,1),2).code==tw::Code::UntrustedSender,"untrusted_state_no_cache");
        std::array<uint8_t,64> malformed{};check(client.runtime.receive(1,malformed.data(),64).code==tw::Code::Malformed,"malformed_packet_rejected");
        check(client.f.writes==0,"early_cache_no_presentation_write");
    }
    {
        Peer host(true);host.hooks();host.send_result=rt::SendResult::Failed;
        check(!host.runtime.tick(host.error)&&host.status().sends_failed==1,"transport_error_retains_packet");
        const auto bytes=host.attempts.back().bytes;
        check(!host.runtime.tick(host.error)&&host.attempts.back().bytes==bytes&&host.status().last_reason_repeats==2,"transport_error_repeat_count_bounded_log");
        check(host.logs==1,"repeated_transport_failure_not_log_spam");
        host.send_result=rt::SendResult::Accepted;host.tick();check(host.status().last_error.empty()&&host.logs==2,"transport_recovery_logged");
    }
    {
        Peer client;client.hooks();client.software_epoch=20;
        check(!client.runtime.tick(client.error)&&client.error=="software_epoch_mismatch"&&client.f.writes==0,"client_software_epoch_guard");
        client.software_epoch=19;client.tick();client.f.put(Fixture::clock+0x58,1.0f);
        auto r=client.receive(client.state(19,1,3));
        check(r.code==tw::Code::PresentationRejected&&client.f.writes==0,"running_client_presentation_rejected_before_store");
    }
    {
        Peer host(true);host.hooks();host.tick();host.session.peer_mask=7;
        check(!host.runtime.tick(host.error)&&host.error=="session_binding_changed","membership_change_retires_epoch");
        check(!host.runtime.tick(host.error)&&host.error=="retired_epoch_requires_new_snapshot","membership_requires_fresh_snapshot");
    }
    {
        Peer host(true);host.hooks();host.tick();host.f.put(Fixture::clock+0x58,3.0f);host.f.put(Fixture::clock+0x5c,0.0f);host.tick();
        check(host.activate(1,0x4a2d93).request_queued,"keyboard_toggle_captured");host.tick();
        check(host.f.get<float>(Fixture::clock+0x58)==1&&host.f.get<float>(Fixture::clock+0x5c)==0,"toggle_uses_host_modal_effective_scale");
        check(host.activate(1,0x12345).decision==Decision::Suppress&&!host.activate(1,0x12345).request_queued,"unknown_ui_origin_not_executed");
        check(host.runtime.set_hook_mask(0,host.error)&&host.status().phase==rt::Phase::Retiring,"hook_removal_retires_epoch");
    }
    {
        Peer host(true),client;host.session.peer_mask=9;client.session.local_peer=4;client.session.peer_mask=9;
        host.hooks();client.hooks();host.tick();client.tick();host.deliver(client);client.tick();client.deliver(host);
        check(client.runtime.submit(tw::Intent::Fastest).okay(),"noncontiguous_peer_mask_submit");client.tick();client.deliver(host);host.tick();
        check(host.set_calls==1,"peer4_authenticated_request_execute");
        const auto forged=client.state(19,1,1);check(host.receive(forged,2).code==tw::Code::UntrustedSender,"absent_peer2_not_inferred_from_count");
    }
    {
        Peer host(true),client;host.hooks();client.hooks();host.tick();client.tick();host.deliver(client);client.tick();client.deliver(host);
        auto m=client.state(19,1,0);m.kind=tw::Kind::Receipt;m.peer=2;m.sequence=1;m.result=tw::Result::NoChange;
        check(client.receive(m).code==tw::Code::UnexpectedReceipt,"unsolicited_receipt_rejected");
        tw::Message saved{};bool found=false;check(client.runtime.observe_receipt(2,1,saved,found,client.error)&&!found,"unsolicited_receipt_not_observable_as_accepted");
    }
    std::cout<<"{\"passed\":"<<passed<<",\"failed\":0,\"runtime_accepted\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: time_runtime fixture: "<<e.what()<<" passed="<<passed<<'\n';return 1;}}
