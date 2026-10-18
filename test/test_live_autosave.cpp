#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include "../src/game/live_host_state/frame_apply_acks.h"
#include "tutorial_replication/state.h"
using mt2mp::game::live_host_state::FrameApplyAcks;
namespace tutorial_replication=mt2mp::game::tutorial_replication;

int assertions=0;
void check(bool v,const char* label){++assertions;if(!v)throw std::runtime_error(label);}
bool fail(std::string& e,const std::string& why){e=why;return false;}
#include "autosave_activity.inc"

namespace action_session {enum class Role {Host,Client};}
struct Context {action_session::Role role=action_session::Role::Host;uint32_t local_peer=1;int id=1;bool operator==(const Context&)const=default;};
struct Binding {uint64_t native_epoch=7,native_generation=8;};
struct Pin {Context context;Binding binding;uint64_t common_epoch=0;uintptr_t image=0,world=0;uint32_t thread=0;};
constexpr const char* image_sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
struct Fixture {bool resolver=true,session=true,world_read=true,status_read=true;uintptr_t image=100;uint32_t thread=2;std::string sha=image_sha;Context context;void* world=reinterpret_cast<void*>(200);void* state=reinterpret_cast<void*>(300);uint64_t epoch=7,generation=8,common=9;int32_t saves=0,edits=0;} fixture_world;
uint32_t GetCurrentThreadId(){return 2;}
void* GetModuleHandleW(const void*){return reinterpret_cast<void*>(100);}
namespace native {
struct WorldStatus {int32_t saves_in_progress=0,edit_operations=0;};
bool read_resolver_receipt(uintptr_t& i,uint32_t& t,std::string& s,std::string& e){if(!fixture_world.resolver)return fail(e,"original resolver read failure");i=fixture_world.image;t=fixture_world.thread;s=fixture_world.sha;return true;}
bool read_world_status(WorldStatus& s,std::string& e){if(!fixture_world.status_read)return fail(e,"original world status failure");s={fixture_world.saves,fixture_world.edits};return true;}
}
namespace action_session {struct Session {bool current(Context& c,std::string& e){if(!fixture_world.session)return fail(e,"original session failure");c=fixture_world.context;return true;}};Session& selected_session(){static Session s;return s;}}
namespace building_mixed_identity::opened_file {struct CurrentWorld {void* world=nullptr;void* state=nullptr;uint64_t world_epoch=0,generation=0;};}
namespace native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& w,std::string& e){if(!fixture_world.world_read)return fail(e,"original opened world failure");w={fixture_world.world,fixture_world.state,fixture_world.epoch,fixture_world.generation};return true;}}
namespace net {uint64_t current_world_epoch(){return fixture_world.common;}}
struct NativePort {Context context_;Binding binding_;bool publication_ready(uint64_t,bool&,std::string&);bool publication_ready(bool& w,std::string& e){return publication_ready(9,w,e);}};
#include "autosave_scope.inc"

enum class Admission {Wait,Rejected,New,Duplicate};
struct Frame {int marketing=0,vips=0,bugs=0,developers=0,network=0,values=0,selected=0,pause=0;std::optional<tutorial_replication::State> tutorial;uint64_t base_action_revision=0,time_revision=1;};
#include "autosave_tutorial_publication.inc"
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
    std::optional<Economy> captured_economy_,last_economy_;
    std::optional<int> queued_day_,queued_economy_;
    std::optional<std::chrono::steady_clock::time_point> last_capture_;
    Day* day_=nullptr;bool economy_=false;uint64_t last_day_sequence_=0,common_epoch_=9,source_generation_=8;
    uint8_t ack_mask=0;uint64_t sent_revision=0;std::chrono::steady_clock::time_point fake_send_time{};
    int captures=0,sends=0,takes=0;bool capture_ok=true,send_ok=true,take_ok=true;Frame next_frame;
    bool capture(uint64_t a,Frame& out,std::string& e){++captures;if(!capture_ok)return fail(e,"original capture failure");out=next_frame;out.base_action_revision=a;out.time_revision=time_.revision;if(economy_)captured_economy_=Economy{};return true;}
    bool send(const Frame& out,std::string& e){++sends;if(!send_ok)return fail(e,"original send failure");last_=out;if(economy_)last_economy_=captured_economy_;if(ack_mask)apply_acks_.sent(++sent_revision,ack_mask,fake_send_time);return true;}
    bool take(const Frame&,std::string& e){++takes;return take_ok?true:fail(e,"original take failure");}
    bool tick(uint64_t,bool,std::string&,std::chrono::steady_clock::time_point);
};
#include "autosave_tick.inc"

#include "frame_apply_ack_cases.inc"
int main(){try{
    std::string e;bool waiting=false;NativePort p;
    for(int saves:{1,2,INT32_MAX}){fixture_world={};fixture_world.saves=saves;e.clear();check(p.publication_ready(waiting,e)&&waiting,"positive native save defers");check(e.find("waits for native save")!=std::string::npos,"wait has measured cause");check(!native_activity(saves,0,0,e),"strict read/store guard remains fatal");}
    fixture_world={};check(p.publication_ready(waiting,e)&&!waiting&&e.empty(),"idle ready clears wait");
    fixture_world.saves=1;fixture_world.common=10;check(!p.publication_ready(waiting,e)&&!waiting&&e=="live publication actual common epoch differs from Bridge","busy cannot hide changed nonzero common epoch");
    for(auto [saves,edits]:{std::pair{1,1},std::pair{1,2},std::pair{1,-1},std::pair{0,1},std::pair{-1,0}}){fixture_world={};fixture_world.saves=saves;fixture_world.edits=edits;e.clear();check(!p.publication_ready(waiting,e)&&!waiting,"edit or invalid save stays fatal");check(e.find("save/edit scope differs")!=std::string::npos,"strict original error retained");}
    for(int bad=0;bad<13;++bad){fixture_world={};fixture_world.saves=1;p={};switch(bad){case 0:fixture_world.resolver=false;break;case 1:fixture_world.thread=3;break;case 2:fixture_world.image=101;break;case 3:fixture_world.sha="other";break;case 4:fixture_world.session=false;break;case 5:fixture_world.context.id=2;break;case 6:fixture_world.world_read=false;break;case 7:fixture_world.world=nullptr;break;case 8:fixture_world.state=nullptr;break;case 9:fixture_world.epoch=6;break;case 10:fixture_world.generation=9;break;case 11:fixture_world.status_read=false;break;case 12:fixture_world.common=0;break;}e.clear();check(!p.publication_ready(waiting,e)&&!waiting&&!e.empty(),"busy cannot conceal owner/read failure");if(bad==11)check(e=="original world status failure","upstream status cause exact");}
    fixture_world={};p.context_.role=action_session::Role::Client;check(!p.publication_ready(waiting,e),"Client publication probe rejected");p={};p.context_.local_peer=2;check(!p.publication_ready(waiting,e),"foreign Host peer rejected");
    auto now=std::chrono::steady_clock::now();
    fixture_world={};Bridge publication;publication.next_frame.tutorial.emplace();publication.next_frame.tutorial->research.emplace();
    check(publication.tick(0,false,e,now)&&publication.sends==1,"initial tutorial publication failed");
    ++publication.next_frame.tutorial->research->value.release_counter;
    check(publication.tick(0,false,e,now+std::chrono::milliseconds(250))&&publication.sends==1,"Research scan stamp alone repeated ordinary publication");
    publication.next_frame.marketing=1;
    check(publication.tick(0,false,e,now+std::chrono::milliseconds(500))&&publication.sends==2,"Marketing change was omitted from ordinary publication");
    publication.next_frame.tutorial->initial_complete=true;
    check(publication.tick(0,false,e,now+std::chrono::milliseconds(750))&&publication.sends==3,"Tutorial value change was ignored with its scan stamp");
    fixture_world={};Bridge cadence;check(cadence.tick(0,false,e,now)&&cadence.captures==1&&cadence.sends==1,"initial world publishes immediately");
    for(int ms:{1,100,249})check(cadence.tick(0,false,e,now+std::chrono::milliseconds(ms))&&cadence.captures==1&&cadence.port_.probes==1,"ordinary capture waits within250ms");
    check(cadence.tick(0,false,e,now+std::chrono::milliseconds(250))&&cadence.captures==2,"ordinary capture resumes at250ms boundary");
    check(cadence.tick(1,false,e,now+std::chrono::milliseconds(251))&&cadence.captures==3&&cadence.last_->base_action_revision==1,"action revision bypasses ordinary wait");
    cadence.time_.revision=2;check(cadence.tick(1,false,e,now+std::chrono::milliseconds(252))&&cadence.captures==4&&cadence.last_->time_revision==2,"time revision bypasses ordinary wait");
    check(cadence.tick(1,false,e,now+std::chrono::milliseconds(501))&&cadence.captures==4,"authority capture starts next ordinary interval");
    check(cadence.tick(1,false,e,now+std::chrono::milliseconds(502))&&cadence.captures==5,"ordinary capture resumes after authority interval");
    fixture_world={};fixture_world.saves=1;Bridge b;check(b.tick(0,false,e,now),"Host ordinary save is successful wait");check(b.port_.probes==1&&b.captures==0&&b.sends==0&&!b.last_&&!b.last_capture_,"save performs no capture/send/revision advance");check(!b.publication_wait_.empty()&&!b.locked_,"wait distinct from failure");
    check(b.tick(0,false,e,now+std::chrono::seconds(1))&&b.captures==0,"persistent save waits without capture");
    const auto first_wait=b.publication_first_wait_;
    check(b.publication_wait_count_==2&&b.publication_recovery_count_==0&&!first_wait.empty(),"wait receipt retains actual observations");
    fixture_world.saves=0;check(b.tick(0,false,e,now+std::chrono::seconds(2)),"idle resumes same Bridge");check(b.captures==1&&b.sends==1&&b.last_&&b.publication_wait_.empty(),"recovery publishes once and clears wait");
    check(b.publication_recovery_count_==1&&b.publication_first_wait_==first_wait&&b.publication_last_wait_==first_wait&&!b.publication_was_waiting_,"idle recovery retains first/last cause");
    fixture_world.saves=1;check(b.tick(0,false,e,now+std::chrono::seconds(3)),"second original save waits");fixture_world.edits=1;check(!b.tick(0,false,e,now+std::chrono::seconds(4))&&b.publication_recovery_count_==1,"real edit failure does not count as save recovery");
    for(int bad=0;bad<6;++bad){fixture_world={};fixture_world.saves=1;Bridge q;switch(bad){case 0:fixture_world.edits=1;break;case 1:fixture_world.epoch=99;break;case 2:q.time_.okay=false;break;case 3:q.locked_=true;break;case 4:q.fragments_.okay=false;break;case 5:fixture_world.status_read=false;break;}e.clear();check(!q.tick(0,false,e,now)&&!e.empty(),"real failures remain failed tick");check(q.captures==0&&q.sends==0,"fatal preflight has no capture/send");}
    fixture_world={};fixture_world.saves=1;Bridge pending;check(pending.tick(1,true,e,now)&&pending.port_.probes==0&&pending.captures==0,"existing pending action wait unchanged");
    fixture_world={};Bridge capture;capture.capture_ok=false;check(!capture.tick(0,false,e,now)&&e=="original capture failure","idle capture failure retained");
    Bridge send;send.send_ok=false;check(!send.tick(0,false,e,now)&&e=="original send failure","send failure retained");
    fixture_world={};fixture_world.saves=1;Bridge client;client.context_.role=action_session::Role::Client;client.queued_=Frame{};check(client.tick(0,false,e,now)&&client.port_.probes==0&&client.takes==1,"Client adoption unchanged, no save bypass");
    frame_ack_cases(now);
    std::cout<<assertions<<" actual autosave publication checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<assertions<<" checks: "<<x.what()<<'\n';return 1;}}
