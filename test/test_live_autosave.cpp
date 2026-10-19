#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include "../src/game/live_host_state/frame_apply_acks.h"
#include "../src/game/tutorial_replication/state.h"
namespace tutorial_replication=mt2mp::game::tutorial_replication;
#include "autosave_tutorial_publication.inc"
using mt2mp::game::live_host_state::FrameApplyAcks;

int assertions=0;
void check(bool v,const char* label){++assertions;if(!v)throw std::runtime_error(label);}
bool fail(std::string& e,const std::string& why){e=why;return false;}
#include "autosave_activity.inc"

namespace action_session {enum class Role {Host,Client};}
struct Context {action_session::Role role=action_session::Role::Host;uint32_t local_peer=1;int id=1;bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=7,native_generation=8;bool operator==(const Binding&)const=default;};
struct Pin {Context context;Binding binding;uint64_t common_epoch=0;uintptr_t image=0,world=0,clock=0,business=0,record=0,history=0;uint32_t thread=0;int32_t history_count=0,history_capacity=0;int values=0;float selected=0,pause=0;bool operator==(const Pin&)const=default;};
constexpr const char* image_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct Fixture {bool resolver=true,session=true,world_read=true,status_read=true;uintptr_t image=100;uint32_t thread=2;std::string sha=image_sha;Context context;void* world=reinterpret_cast<void*>(200);void* state=reinterpret_cast<void*>(300);uint64_t epoch=7,generation=8,common=9;int32_t saves=0,edits=0;} world_fixture;
uint32_t GetCurrentThreadId(){return 2;}
void* GetModuleHandleW(const void*){return reinterpret_cast<void*>(100);}
namespace native {
struct WorldStatus {int32_t saves_in_progress=0,edit_operations=0;};
bool read_resolver_receipt(uintptr_t& i,uint32_t& t,std::string& s,std::string& e){if(!world_fixture.resolver)return fail(e,"original resolver read failure");i=world_fixture.image;t=world_fixture.thread;s=world_fixture.sha;return true;}
bool read_world_status(WorldStatus& s,std::string& e){if(!world_fixture.status_read)return fail(e,"original world status failure");s={world_fixture.saves,world_fixture.edits};return true;}
}
namespace action_session {struct Session {bool current(Context& c,std::string& e){if(!world_fixture.session)return fail(e,"original session failure");c=world_fixture.context;return true;}};Session& selected_session(){static Session s;return s;}}
namespace building_mixed_identity::opened_file {struct CurrentWorld {void* world=nullptr;void* state=nullptr;uint64_t world_epoch=0,generation=0;};}
namespace native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& w,std::string& e){if(!world_fixture.world_read)return fail(e,"original opened world failure");w={world_fixture.world,world_fixture.state,world_fixture.epoch,world_fixture.generation};return true;}}
namespace net {uint64_t current_world_epoch(){return world_fixture.common;}}
struct EconomicBoundary {int reads=0;bool okay=true;bool read_current_fence(const Pin&,std::string& e){++reads;return okay||fail(e,"original economic fence failed");}};
struct NativePort {Context context_;Binding binding_;EconomicBoundary* boundary_=nullptr;bool current_owners(const Pin&,std::string&,int32_t=0);bool store(const Pin&,size_t,int32_t,int32_t,std::string&);bool publication_ready(uint64_t,bool&,std::string&);bool publication_ready(bool& w,std::string& e){return publication_ready(9,w,e);}};
#include "autosave_scope.inc"

constexpr size_t scalar_count=52;
Pin owner_sample;bool owner_read_ok=true;int owner_reads=0,scalar_stores=0;
bool once(const Context&,const Binding&,Pin& p,float& pause,std::string& e,int32_t expected_edits=0){
    ++owner_reads;if(!owner_read_ok)return fail(e,"original scalar owner read failed");
    if(!native_activity(world_fixture.saves,world_fixture.edits,expected_edits,e))return false;
    p=owner_sample;pause=p.pause;return true;
}
uintptr_t address(const Pin&,size_t slot){return 0x1000+slot*4;}
bool compare_fixed_i32(uintptr_t,int32_t,int32_t,std::string& e){++scalar_stores;e.clear();return true;}
#include "autosave_owners.inc"
void scalar_owner_cases(){
    world_fixture={};NativePort port;EconomicBoundary fence;port.boundary_=&fence;
    Pin pin;pin.context=port.context_;pin.binding=port.binding_;pin.thread=2;pin.common_epoch=9;
    pin.image=100;pin.world=200;pin.clock=300;pin.business=400;pin.record=500;pin.history=600;pin.history_count=1;pin.history_capacity=4;pin.pause=1;
    std::string e;
    for(float scale:{0.f,1.f,3.f,9.f}){pin.selected=scale;owner_sample=pin;
        check(port.current_owners(pin,e),"Host capture keeps identical current authority scale0/1/3/9");
        owner_sample.values=123;check(port.current_owners(pin,e),"owner guard excludes values while preserving owner and Clock identity");
        scalar_stores=0;fence.reads=0;check(!port.store(pin,0,0,1,e)&&scalar_stores==0&&fence.reads==0,"Host owner acceptance never permits Client scalar store");
    }
    pin.selected=3;owner_sample=pin;owner_sample.selected=1;check(!port.current_owners(pin,e),"Host selected scale change still rejects capture");
    for(int change=0;change<14;++change){owner_sample=pin;switch(change){case 0:++owner_sample.image;break;case 1:++owner_sample.world;break;case 2:++owner_sample.clock;break;case 3:++owner_sample.business;break;case 4:++owner_sample.record;break;case 5:++owner_sample.history;break;case 6:++owner_sample.history_count;break;case 7:++owner_sample.history_capacity;break;case 8:++owner_sample.thread;break;case 9:++owner_sample.common_epoch;break;case 10:++owner_sample.context.id;break;case 11:++owner_sample.binding.native_epoch;break;case 12:++owner_sample.binding.native_generation;break;case 13:owner_sample.pause=0;break;}
        check(!port.current_owners(pin,e),"Host actual owner/History/pause identity mismatch stays rejected");}
    owner_sample=pin;owner_read_ok=false;check(!port.current_owners(pin,e)&&e=="original scalar owner read failed","owner guard retains upstream read failure");owner_read_ok=true;
    world_fixture.saves=1;check(!port.current_owners(pin,e),"Host running capture cannot bypass native save guard");world_fixture={};
    port.context_.role=action_session::Role::Client;port.context_.local_peer=2;pin.context=port.context_;
    for(float scale:{0.f,1.f,3.f,9.f}){pin.selected=scale;owner_sample=pin;scalar_stores=0;fence.reads=0;
        check(port.current_owners(pin,e)==(scale==0),"Client owner guard remains Clock0 only even when expected pin has same scale");
        check(port.store(pin,0,0,1,e)==(scale==0),"actual store remains Client Clock0 only");
        check(scalar_stores==(scale==0?1:0)&&fence.reads==(scale==0?1:0),"rejected nonzero Client clock cannot reach fence or scalar store");
    }
    pin.selected=0;owner_sample=pin;owner_sample.selected=3;scalar_stores=0;check(!port.store(pin,0,0,1,e)&&scalar_stores==0,"Client clock change after initial pin prevents actual scalar store");
    owner_sample=pin;port.boundary_=nullptr;check(!port.store(pin,0,0,1,e)&&scalar_stores==0,"missing Client fence still prevents actual scalar store");port.boundary_=&fence;
    fence.okay=false;check(!port.store(pin,0,0,1,e)&&e=="original economic fence failed"&&scalar_stores==0,"fence failure propagates without store");fence.okay=true;
    check(!port.store(pin,scalar_count,0,1,e)&&scalar_stores==0,"out-of-range scalar store still rejected");
    world_fixture={};owner_read_ok=true;
}


enum class Admission {Wait,Rejected,New,Duplicate};
struct Frame {int management=0,monsters=0,marketing=0,vips=0,bugs=0,developers=0,network=0,values=0,selected=0,pause=0;std::optional<tutorial_replication::State> tutorial;uint64_t base_action_revision=0,time_revision=1;};
struct Economy {int subscribers=0,loans=0,offers=0,npcs=0,additions=0;};
struct Day {uint64_t sequence(){return 1;}};
struct Time {bool okay=true;uint64_t revision=1;explicit operator bool()const{return true;}bool operator()(uint64_t& n,std::string& e){n=revision;return okay?true:fail(e,"original time failure");}};
struct Fragment {bool okay=true;bool timely(std::chrono::steady_clock::time_point,std::string& e){return okay?true:fail(e,"original fragment failure");}};
Admission admission_result=Admission::New;
Admission admit(int,const Context&,uint64_t,uint64_t,const Frame*,const Frame&,uint64_t,uint64_t,bool,std::string& e){if(admission_result==Admission::Rejected)e="original admission failure";return admission_result;}
struct BridgeContext:Context {int host_peer=1;};
struct Port {NativePort native;int probes=0;bool publication_ready(uint64_t epoch,bool& w,std::string& e){++probes;return native.publication_ready(epoch,w,e);}};
struct Bridge {
    std::string publication_wait_;bool suspended_=false,locked_=false;
    std::string publication_first_wait_,publication_last_wait_;
    uint64_t publication_wait_count_=0,publication_recovery_count_=0;
    bool publication_was_waiting_=false;
    FrameApplyAcks apply_acks_;
    Fragment fragments_;BridgeContext context_;Port port_;Time time_;
    std::optional<Frame> last_,queued_;
    Frame captured_frame;
    std::optional<Economy> captured_economy_,last_economy_;
    std::optional<int> queued_day_,queued_economy_;
    std::optional<std::chrono::steady_clock::time_point> last_capture_;
    Day* day_=nullptr;bool economy_=false;uint64_t last_day_sequence_=0,common_epoch_=9,source_generation_=8;
    uint8_t ack_mask=0;uint64_t sent_revision=0;std::chrono::steady_clock::time_point fake_send_time{};
    int captures=0,sends=0,takes=0;bool capture_ok=true,send_ok=true,take_ok=true;
    bool capture(uint64_t a,Frame& out,std::string& e){++captures;if(!capture_ok)return fail(e,"original capture failure");out=captured_frame;out.base_action_revision=a;out.time_revision=time_.revision;if(economy_)captured_economy_=Economy{};return true;}
    bool send(const Frame& out,std::string& e){++sends;if(!send_ok)return fail(e,"original send failure");last_=out;if(economy_)last_economy_=captured_economy_;if(ack_mask)apply_acks_.sent(++sent_revision,ack_mask,fake_send_time);return true;}
    bool take(const Frame&,std::string& e){++takes;return take_ok?true:fail(e,"original take failure");}
    bool tick(uint64_t,bool,std::string&,std::chrono::steady_clock::time_point);
};
#include "autosave_tick.inc"

void publication_payload_cases(std::chrono::steady_clock::time_point now){
    world_fixture={};std::string e;
    for(auto field:{&Frame::management,&Frame::monsters,&Frame::marketing,&Frame::vips,&Frame::bugs,&Frame::developers,&Frame::network,&Frame::values,&Frame::selected,&Frame::pause}){
        Bridge bridge;
        check(bridge.tick(0,false,e,now)&&bridge.sends==1,"initial payload publishes");
        ++(bridge.captured_frame.*field);
        check(bridge.tick(0,false,e,now+std::chrono::milliseconds(250))&&bridge.sends==2,"changed compared payload publishes at ordinary cadence");
        check(bridge.tick(0,false,e,now+std::chrono::milliseconds(500))&&bridge.sends==2,"identical compared payload suppresses duplicate publication");
    }
    Bridge tutorial;
    tutorial.captured_frame.tutorial=tutorial_replication::State{};
    tutorial.captured_frame.tutorial->research=tutorial_replication::research::Row{};
    tutorial.captured_frame.tutorial->research->key="bridges";
    tutorial.captured_frame.tutorial->research->value.release_counter=7413;
    check(tutorial.tick(0,false,e,now)&&tutorial.sends==1,"initial real tutorial payload publishes");
    tutorial.captured_frame.tutorial->research->value.release_counter=8279;
    check(tutorial.tick(0,false,e,now+std::chrono::milliseconds(250))&&tutorial.sends==1,"actual tutorial predicate suppresses only changed Research scan stamp");
    check(tutorial.last_->tutorial->research->value.release_counter==7413&&tutorial.captured_frame.tutorial->research->value.release_counter==8279,"publication comparison does not rewrite retained or captured payload");
    tutorial.captured_frame.tutorial->initial_complete=true;
    check(tutorial.tick(0,false,e,now+std::chrono::milliseconds(500))&&tutorial.sends==2,"semantic tutorial change still publishes");
    tutorial.captured_frame.tutorial->startingpoint=tutorial_replication::startingpoint::Row{};
    check(tutorial.tick(0,false,e,now+std::chrono::milliseconds(750))&&tutorial.sends==3,"Startingpoint presence still publishes");
    ++tutorial.captured_frame.tutorial->startingpoint->release_counter;
    check(tutorial.tick(0,false,e,now+std::chrono::milliseconds(1000))&&tutorial.sends==4,"Startingpoint stamp remains an exact publication input");
    tutorial.captured_frame.tutorial.reset();
    check(tutorial.tick(0,false,e,now+std::chrono::milliseconds(1250))&&tutorial.sends==5,"tutorial removal publishes");
    check(tutorial.tick(0,false,e,now+std::chrono::milliseconds(1500))&&tutorial.sends==5,"two absent tutorial payloads suppress duplicate publication");
}

#include "frame_apply_ack_cases.inc"
int main(){try{
    std::string e;bool waiting=false;NativePort p;
    for(int saves:{1,2,INT32_MAX}){world_fixture={};world_fixture.saves=saves;e.clear();check(p.publication_ready(waiting,e)&&waiting,"positive native save defers");check(e.find("waits for native save")!=std::string::npos,"wait has measured cause");check(!native_activity(saves,0,0,e),"strict read/store guard remains fatal");}
    world_fixture={};check(p.publication_ready(waiting,e)&&!waiting&&e.empty(),"idle ready clears wait");
    world_fixture.saves=1;world_fixture.common=10;check(!p.publication_ready(waiting,e)&&!waiting&&e=="live publication actual common epoch differs from Bridge","busy cannot hide changed nonzero common epoch");
    for(auto [saves,edits]:{std::pair{1,1},std::pair{1,2},std::pair{1,-1},std::pair{0,1},std::pair{-1,0}}){world_fixture={};world_fixture.saves=saves;world_fixture.edits=edits;e.clear();check(!p.publication_ready(waiting,e)&&!waiting,"edit or invalid save stays fatal");check(e.find("save/edit scope differs")!=std::string::npos,"strict original error retained");}
    for(int bad=0;bad<13;++bad){world_fixture={};world_fixture.saves=1;p={};switch(bad){case 0:world_fixture.resolver=false;break;case 1:world_fixture.thread=3;break;case 2:world_fixture.image=101;break;case 3:world_fixture.sha="other";break;case 4:world_fixture.session=false;break;case 5:world_fixture.context.id=2;break;case 6:world_fixture.world_read=false;break;case 7:world_fixture.world=nullptr;break;case 8:world_fixture.state=nullptr;break;case 9:world_fixture.epoch=6;break;case 10:world_fixture.generation=9;break;case 11:world_fixture.status_read=false;break;case 12:world_fixture.common=0;break;}e.clear();check(!p.publication_ready(waiting,e)&&!waiting&&!e.empty(),"busy cannot conceal owner/read failure");if(bad==11)check(e=="original world status failure","upstream status cause exact");}
    world_fixture={};p.context_.role=action_session::Role::Client;check(!p.publication_ready(waiting,e),"Client publication probe rejected");p={};p.context_.local_peer=2;check(!p.publication_ready(waiting,e),"foreign Host peer rejected");
    auto now=std::chrono::steady_clock::now();
    world_fixture={};Bridge cadence;check(cadence.tick(0,false,e,now)&&cadence.captures==1&&cadence.sends==1,"initial world publishes immediately");
    for(int ms:{1,100,249})check(cadence.tick(0,false,e,now+std::chrono::milliseconds(ms))&&cadence.captures==1&&cadence.port_.probes==1,"ordinary capture waits within250ms");
    check(cadence.tick(0,false,e,now+std::chrono::milliseconds(250))&&cadence.captures==2,"ordinary capture resumes at250ms boundary");
    check(cadence.tick(1,false,e,now+std::chrono::milliseconds(251))&&cadence.captures==3&&cadence.last_->base_action_revision==1,"action revision bypasses ordinary wait");
    cadence.time_.revision=2;check(cadence.tick(1,false,e,now+std::chrono::milliseconds(252))&&cadence.captures==4&&cadence.last_->time_revision==2,"time revision bypasses ordinary wait");
    check(cadence.tick(1,false,e,now+std::chrono::milliseconds(501))&&cadence.captures==4,"authority capture starts next ordinary interval");
    check(cadence.tick(1,false,e,now+std::chrono::milliseconds(502))&&cadence.captures==5,"ordinary capture resumes after authority interval");
    world_fixture={};world_fixture.saves=1;Bridge b;check(b.tick(0,false,e,now),"Host ordinary save is successful wait");check(b.port_.probes==1&&b.captures==0&&b.sends==0&&!b.last_&&!b.last_capture_,"save performs no capture/send/revision advance");check(!b.publication_wait_.empty()&&!b.locked_,"wait distinct from failure");
    check(b.tick(0,false,e,now+std::chrono::seconds(1))&&b.captures==0,"persistent save waits without capture");
    const auto first_wait=b.publication_first_wait_;
    check(b.publication_wait_count_==2&&b.publication_recovery_count_==0&&!first_wait.empty(),"wait receipt retains actual observations");
    world_fixture.saves=0;check(b.tick(0,false,e,now+std::chrono::seconds(2)),"idle resumes same Bridge");check(b.captures==1&&b.sends==1&&b.last_&&b.publication_wait_.empty(),"recovery publishes once and clears wait");
    check(b.publication_recovery_count_==1&&b.publication_first_wait_==first_wait&&b.publication_last_wait_==first_wait&&!b.publication_was_waiting_,"idle recovery retains first/last cause");
    world_fixture.saves=1;check(b.tick(0,false,e,now+std::chrono::seconds(3)),"second original save waits");world_fixture.edits=1;check(!b.tick(0,false,e,now+std::chrono::seconds(4))&&b.publication_recovery_count_==1,"real edit failure does not count as save recovery");
    for(int bad=0;bad<6;++bad){world_fixture={};world_fixture.saves=1;Bridge q;switch(bad){case 0:world_fixture.edits=1;break;case 1:world_fixture.epoch=99;break;case 2:q.time_.okay=false;break;case 3:q.locked_=true;break;case 4:q.fragments_.okay=false;break;case 5:world_fixture.status_read=false;break;}e.clear();check(!q.tick(0,false,e,now)&&!e.empty(),"real failures remain failed tick");check(q.captures==0&&q.sends==0,"fatal preflight has no capture/send");}
    world_fixture={};world_fixture.saves=1;Bridge pending;check(pending.tick(1,true,e,now)&&pending.port_.probes==0&&pending.captures==0,"existing pending action wait unchanged");
    world_fixture={};Bridge capture;capture.capture_ok=false;check(!capture.tick(0,false,e,now)&&e=="original capture failure","idle capture failure retained");
    Bridge send;send.send_ok=false;check(!send.tick(0,false,e,now)&&e=="original send failure","send failure retained");
    world_fixture={};world_fixture.saves=1;Bridge client;client.context_.role=action_session::Role::Client;client.queued_=Frame{};check(client.tick(0,false,e,now)&&client.port_.probes==0&&client.takes==1,"Client adoption unchanged, no save bypass");
    publication_payload_cases(now);
    scalar_owner_cases();
    frame_ack_cases(now);
    std::cout<<assertions<<" actual autosave publication checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<assertions<<" checks: "<<x.what()<<'\n';return 1;}}
