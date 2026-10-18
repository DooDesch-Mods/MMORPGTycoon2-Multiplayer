#include "vip_bridge.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace action=mt2mp::action_session;
namespace vip=live::vip;
namespace mt2mp::game::live_host_state {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
#include "vip_failure_latch.inc"
#include "vip_bridge_methods.inc"
}
namespace {
size_t checks=0;
void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
struct Port:live::Port {
    bool read(live::Pin&,std::string& e)override{e="unexpected native read";return false;}
    bool store(const live::Pin&,size_t,int32_t,int32_t,std::string& e)override{e="unexpected native store";return false;}
};
struct Transport:action::Transport {
    bool claim(Handler,std::string& e)override{e="unexpected transport claim";return false;}
    bool send(uint64_t,const std::vector<uint8_t>&,std::string& e)override{e="unexpected send";return false;}
};
live::Context context(bool host){return {41,host?1u:2u,1,3,host?action::Role::Host:action::Role::Client,"11111111-1111-4111-8111-111111111111"};}
vip::Origin origin(){return {41,7,9,"11111111-1111-4111-8111-111111111111"};}
live::Frame frame(const vip::Batch& batch,uint64_t revision=1){
    live::Frame f;const auto o=origin();f.session=o.session;f.epoch=o.epoch;f.source_generation=o.generation;f.snapshot=o.snapshot;f.revision=revision;f.vips=batch;return f;
}
vip::Transition arrival(uint64_t sequence=1,int32_t id=151){return {sequence,sequence,id,"Visitor",true};}
struct Vip:live::VipPort {
    unsigned binds=0,captures=0,sends=0,acks=0,suspends=0,resets=0,preflights=0,applies=0;
    uint32_t peer=0;uint64_t revision=0;vip::Origin observed;
    vip::Batch pending;
    int failure=0;bool attempt=true,unknown=false,confirmed=true,throw_apply=false;
    bool partial=false;std::vector<int> order;
    bool result(int phase,std::string& e){if(failure==phase){e="original VIP phase "+std::to_string(phase);return false;}e.clear();return true;}
    bool bind(const live::Context&,const live::Binding&,uint64_t,std::string& e)override{++binds;order.push_back(1);return result(1,e);}
    bool host_capture(const vip::Origin& o,vip::Batch& b,std::string& e)override{++captures;observed=o;b=pending;return result(2,e);}
    bool host_sent(const vip::Origin& o,uint64_t r,const vip::Batch& b,std::string& e)override{++sends;observed=o;revision=r;need(b==pending,"captured batch changed before sent");return result(3,e);}
    bool host_acknowledge(const vip::Origin& o,uint32_t p,uint64_t r,std::string& e)override{++acks;observed=o;peer=p;revision=r;return result(4,e);}
    bool suspend(const live::Context&,const live::Binding&,uint64_t,const std::optional<vip::Origin>& o,std::string& e)override{++suspends;order.push_back(5);if(o)observed=*o;return result(5,e);}
    bool reset(const vip::Origin& o,std::string& e)override{++resets;order.push_back(6);observed=o;return result(6,e);}
    bool client_preflight(const vip::Origin& o,const std::vector<vip::Transition>&,std::string& e)override{++preflights;observed=o;order.push_back(7);return result(7,e);}
    bool client_apply(const vip::Origin& o,const std::vector<vip::Transition>& work,live::VipApplyReceipt& out,std::string& e)override{
        ++applies;observed=o;order.push_back(8);out.attempted=attempt&&!work.empty();out.confirmed=confirmed;out.unknown=unknown;
        out.applied=work;if(partial&&!out.applied.empty())out.applied.pop_back();
        if(throw_apply){e=out.cause="original VIP apply exception";throw std::runtime_error(e);}
        const bool ok=result(8,e);out.cause=e;return ok;
    }
};
struct Fixture {
    Port port;Transport transport;Vip adapter;
    live::Bridge bridge;
    explicit Fixture(bool host=false):bridge(context(host),{7,9},7,port,transport,{}){
        // These methods test only presence of the already bound Economy provider.
        // The token is never dereferenced and represents no native pointer.
        bridge.economy_=reinterpret_cast<live::day::economy::Coupling*>(uintptr_t(1));
    }
    void bind(){std::string e;need(bridge.bind_vip(adapter,e),e);}
    live::Frame admit(const vip::Batch& b){auto f=frame(b);std::string e;need(bridge.admit_vips(f,e),e);return f;}
};
void binding_cases(){
    std::string e;
    for(int guard=0;guard<9;++guard){Fixture f;switch(guard){case 0:f.bridge.economy_=nullptr;break;case 1:f.bridge.suspended_=true;break;case 2:f.bridge.locked_=true;break;case 3:f.bridge.last_=live::Frame{};break;case 4:f.bridge.queued_=live::Frame{};break;case 5:f.bridge.host_before_=live::Frame{};break;case 6:f.bridge.host_revision_=1;break;case 7:f.bridge.source_generation_=1;break;case 8:f.bind();break;}
        const auto prior=f.adapter.binds;need(!f.bridge.bind_vip(f.adapter,e)&&f.adapter.binds==prior,"invalid/prior binding reached native adapter");}
    {Fixture f;f.adapter.failure=1;need(!f.bridge.bind_vip(f.adapter,e)&&!f.bridge.vip_port_&&e=="original VIP phase 1","bind failure armed Bridge");}
    {Fixture f(true);f.bind();need(f.bridge.vip_origin_==origin(),"Host origin absent at bind");}
    {Fixture f;f.bind();need(!f.bridge.vip_origin_,"Client assumed local generation is Host origin");}
}
void host_cases(){
    Fixture f(true);f.bind();std::string e;auto out=frame({});out.vips.reset();
    f.adapter.pending={0,{arrival()}};
    need(f.bridge.capture_vips(out,e)&&out.vips==f.adapter.pending&&f.adapter.observed==origin(),"Host capture omitted retained transitions");
    need(f.bridge.sent_vips(out,e)&&f.adapter.revision==1&&f.adapter.sends==1,"Host sent does not cover exact Frame");
    need(f.bridge.acknowledge_vips(2,1,e)&&f.adapter.peer==2&&f.adapter.revision==1&&f.adapter.observed==origin(),"authenticated ACK origin/revision lost");
    for(int phase:{2,3,4}){f.adapter.failure=phase;bool ok=phase==2?f.bridge.capture_vips(out,e):(phase==3?f.bridge.sent_vips(out,e):f.bridge.acknowledge_vips(2,1,e));need(!ok&&e=="original VIP phase "+std::to_string(phase),"Host upstream failure lost");}
    f.adapter.failure=0;f.adapter.pending.rows[0].sequence=9;need(!f.bridge.capture_vips(out,e),"Host invalid native batch encoded");
    out.source_generation++;const auto prior=f.adapter.sends;need(!f.bridge.sent_vips(out,e)&&f.adapter.sends==prior,"changed origin reached native sent provider");
}
void admission_cases(){
    Fixture f;f.bind();std::string e;vip::Batch batch{0,{arrival()}};auto first=f.admit(batch);
    need(f.bridge.vip_origin_==origin()&&f.bridge.vip_client_.committed()==0&&!f.adapter.preflights&&!f.adapter.applies,"portable admission performed UI effect/confirmation");
    f.bridge.queued_=first;
    auto next=frame({0,{arrival(),{2,1,151,"Visitor",false}}},2);
    need(f.bridge.admit_vips(next,e),"arrival+retirement did not supersede intact prefix: "+e);
    need(f.bridge.admit_vips(first,e),"identical queued Frame VIP rejected");
    for(int wrong=0;wrong<4;++wrong){auto bad=next;switch(wrong){case 0:bad.vips->rows[0].name="Other";bad.vips->rows[1].name="Other";break;case 1:bad.vips->rows.clear();break;case 2:bad.vips->after=1;bad.vips->rows.erase(bad.vips->rows.begin());break;case 3:bad.source_generation++;break;}
        need(!f.bridge.admit_vips(bad,e)&&f.bridge.vip_client_.committed()==0&&!f.adapter.applies,"supersession lost or conflicted unACKed transition");}
    std::vector<vip::Transition> work;auto missing=first;missing.vips.reset();need(!f.bridge.prepare_vips(missing,work,e),"armed missing component admitted");
    Fixture legacy;need(!legacy.bridge.prepare_vips(first,work,e),"unarmed VIP component admitted");
    missing.vips.reset();need(legacy.bridge.prepare_vips(missing,work,e)&&work.empty(),"unarmed legacy frame changed");
    need(legacy.bridge.capture_vips(missing,e)&&!missing.vips&&legacy.bridge.sent_vips(missing,e)&&legacy.bridge.acknowledge_vips(2,1,e)&&!legacy.adapter.captures&&!legacy.adapter.sends&&!legacy.adapter.acks,"unarmed legacy path calls native VIP adapter");
}
void apply_cases(){
    std::string e;
    {Fixture f;f.bind();auto target=f.admit({0,{arrival()}});std::vector<vip::Transition> work;need(f.bridge.prepare_vips(target,work,e)&&work==target.vips->rows,"prepare work differs");bool attempted=false;
        need(f.bridge.apply_vips(target,work,attempted,e)&&attempted&&f.bridge.vip_client_.committed()==1,"native success not committed");
        need(f.adapter.order==std::vector<int>({1,7,8})&&f.bridge.vip_status().at("last_apply").at("confirmed")==true&&f.bridge.vip_status().at("last_apply").at("frame_revision")==1,"native order/receipt lost");
        need(f.bridge.prepare_vips(target,work,e)&&work.empty(),"duplicate replays transitions");
        attempted=false;need(f.bridge.apply_vips(target,work,attempted,e)&&!attempted&&f.bridge.vip_client_.committed()==1,"duplicate fabricated effect");}
    for(int phase=0;phase<7;++phase){Fixture f;f.bind();auto target=f.admit({0,{arrival()}});std::vector<vip::Transition> work;need(f.bridge.prepare_vips(target,work,e),e);
        switch(phase){case 0:f.adapter.failure=7;break;case 1:f.adapter.failure=8;break;case 2:f.adapter.confirmed=false;break;case 3:f.adapter.unknown=true;break;case 4:f.adapter.partial=true;break;case 5:f.adapter.failure=7;f.bridge.receipt_.attempted=true;break;case 6:f.adapter.failure=8;f.adapter.attempt=false;break;}
        bool attempted=false;need(!f.bridge.apply_vips(target,work,attempted,e)&&f.bridge.vip_client_.committed()==0&&!f.bridge.receipt_.confirmed&&!e.empty(),"partial native UI confirmed portable transitions");
        need(f.bridge.locked_==(phase!=0&&phase!=6),"pre-effect reject and post-effect Unknown conflated");
        if(phase==0||phase==5)need(f.adapter.applies==0,"native apply after failed preflight");}
    {Fixture f;f.bind();auto target=f.admit({0,{arrival(),{2,1,151,"Visitor",false}}});std::vector<vip::Transition> work;need(f.bridge.prepare_vips(target,work,e),e);f.adapter.attempt=false;bool attempted=false;
        need(f.bridge.apply_vips(target,work,attempted,e)&&!attempted&&f.bridge.vip_client_.committed()==2&&f.bridge.vip_status().at("last_apply").at("applied_count")==2,"expired-before-presentation falsely shown or lost");}
    for(bool exception:{false,true}){Fixture f;f.bind();auto target=f.admit({0,{arrival()}});std::vector<vip::Transition> work;need(f.bridge.prepare_vips(target,work,e),e);bool attempted=false,completed=false,caught=false;f.adapter.throw_apply=exception;
        {live::VipFailureLatch latch{attempted,completed,f.bridge.locked_,f.bridge.receipt_,e};
            try{need(f.bridge.apply_vips(target,work,attempted,e),e);e="original later Frame ACK send failure";}catch(const std::runtime_error&){caught=true;}}
        need(f.bridge.locked_&&f.bridge.receipt_.attempted&&f.bridge.receipt_.unknown&&!f.bridge.receipt_.confirmed,"effect exception/later ACK failure escaped latch");
        need(caught==exception&&f.bridge.receipt_.cause==e,"original post-effect cause lost");}
}
void lifetime_cases(){
    std::string e;
    for(bool host:{false,true}){Fixture f(host);f.bind();if(!host)(void)f.admit({0,{arrival()}});const auto o=f.bridge.vip_origin_;
        need(f.bridge.suspend_vips(e)&&f.adapter.suspends==1&&f.adapter.observed==*o,"suspend lacks exact old origin");
        auto next=context(host);next.snapshot_uuid="22222222-2222-4222-8222-222222222222";
        need(f.bridge.reset_vips(next,{8,10},8,e)&&f.adapter.resets==1&&f.adapter.binds==2&&f.adapter.order==std::vector<int>({1,5,6,1}),"reset/rebind ordering changed");
        need(f.bridge.vip_client_.committed()==0&&f.bridge.vip_status().at("last_apply").is_null(),"snapshot retains old confirmation/status");
        if(host)need(f.bridge.vip_origin_==vip::Origin{41,8,10,next.snapshot_uuid},"Host rebind origin stale");else need(!f.bridge.vip_origin_,"Client rebind assumes local generation");}
    {Fixture f;f.bind();need(f.bridge.suspend_vips(e)&&!f.bridge.vip_origin_,"Client before first Frame cannot suspend");}
    for(int phase:{5,6,1}){Fixture f(true);f.bind();f.adapter.failure=phase;const bool ok=phase==5?f.bridge.suspend_vips(e):f.bridge.reset_vips(context(true),{8,10},8,e);
        need(!ok&&e=="original VIP phase "+std::to_string(phase),"lifetime upstream cause suppressed");}
    {Fixture f;f.bind();(void)f.admit({0,{arrival()}});f.adapter.failure=5;f.bridge.locked_=true;
        need(f.bridge.suspend_lobby(e)&&f.bridge.suspended_&&f.bridge.lobby_retiring_&&f.bridge.locked_,"VIP cleanup failure trapped explicit Leave");
        need(e=="warn: live lobby retiring after VIP cleanup failure original=original VIP phase 5"&&f.bridge.receipt_.unknown&&!f.bridge.receipt_.confirmed&&f.bridge.receipt_.cause=="original VIP phase 5"&&f.bridge.vip_status().at("cleanup").at("unknown")==true,"degraded Leave hid original cleanup Unknown");}
    {Fixture f;f.bind();f.bridge.host_before_=live::Frame{};need(!f.bridge.suspend_lobby(e)&&!f.adapter.suspends&&!f.bridge.suspended_,"active action-before owner cleaned up");}
    {Fixture f;f.bind();f.adapter.failure=5;f.bridge.locked_=true;f.bridge.receipt_.cause="original Economy failure";
        need(f.bridge.suspend_lobby(e)&&f.bridge.receipt_.cause=="original Economy failure"&&f.bridge.vip_status().at("cleanup").at("cause")=="original VIP phase 5","VIP cleanup replaced original upstream failure");}
}
}
int main(){try{binding_cases();host_cases();admission_cases();apply_cases();lifetime_cases();std::cout<<checks<<" actual VIP Bridge wiring checks passed\n";return 0;}catch(const std::exception& x){std::cerr<<"error after "<<checks<<" checks: "<<x.what()<<'\n';return 1;}}
