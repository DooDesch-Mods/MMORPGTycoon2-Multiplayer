#include "monster_remove_fixture.h"
#include "ui_remove/native_contract.h"
#include <cstring>
#include <iostream>
namespace ui=mt2mp::action_session::ui_capture;
namespace mt2mp::action_session::ui_capture {
namespace {
struct Call{uintptr_t pc,target;Origin origin;};
#include "remove_ui_calls_body.inc"
bool frame_ok=true,frame_checked=false;
bool monster_frames(Memory&,uintptr_t,std::string& e){frame_checked=true;if(!frame_ok)e="pinned Monster Command body differs";return frame_ok;}
}
#include "remove_origin_body.inc"
}
struct Bytes:ui::Memory {
 std::map<uintptr_t,uintptr_t> data;
 bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{auto f=data.find(p);if(f==data.end()||n!=8){e="frame unreadable";return false;}std::memcpy(out,&f->second,n);return true;}
 bool region(uintptr_t,ui::Region&,std::string&)override{return false;}
};
struct Services:ui::Services {
 ui::LocalBinding binding{10,20,0x100000001ull,3,4,snapshot_id,std::string("action:")+birth_id,false,false,false,true};
 unsigned calls=0,monsters=0,npcs=0,props=0,rejections=0,zones=0;bool authorize=true,thread=true;ui::Policy mode=ui::Policy::AuthorityRequired;
 bool on_game_thread()noexcept override{return thread;}
 ui::Policy policy(std::string&)override{return mode;}
 bool array(ui::NativePropArray* a,ui::ArrayObservation& o,std::string&)override{o={a->addresspoint,uintptr_t(a->data),a->count?*a->data:0,a->count,a->capacity};return true;}
 bool resolve(uintptr_t,uintptr_t,ui::LocalBinding& o,std::string&)override{o=binding;return true;}
 bool fresh_action_uuid(std::string& o,std::string&)override{o=action_id;return true;}
 bool submit(const ui::Intent&,std::string&)override{++props;return true;}
 bool submit_npc(const ui::Intent&,std::string&)override{++npcs;return true;}
 bool submit_monster(const ui::Intent&,std::string&)override{++monsters;return true;}
 bool submit_zone(const ui::Intent&,std::string&)override{++zones;return true;}
 bool authorize_replay(const ui::Replay&,std::string&)override{return authorize;}
 void original(ui::DestroyProps,void*,ui::NativePropArray*)override{++calls;}
 void log(const char*,const char*,const std::string&)noexcept override{}
 void show_rejection(const std::string&)noexcept override{++rejections;}
};
void original(void*,ui::NativePropArray*){}
int main(){try{std::string e;unsigned n=0;constexpr uintptr_t image=0x10000000,R=0x700000;
 Bytes bytes;bytes.data={{R,image+0x2d55a8},{R+0xa0,image+0x3bcb34},{R+0x2d0,image+0x749454}};
 ui::Origin origin;need(ui::classify_origin(bytes,image,R,origin,e)&&origin==ui::Origin::MonsterWindow&&ui::frame_checked,"exact Monster frame chain missed");++n;
 bytes.data[R+0x2d0]=image+0x749455;ui::frame_checked=false;need(ui::classify_origin(bytes,image,R,origin,e)&&origin==ui::Origin::Foreign&&!ui::frame_checked,"unrelated Monster Command caller accepted");++n;
 bytes.data[R+0x2d0]=image+0x749454;ui::frame_ok=false;need(!ui::classify_origin(bytes,image,R,origin,e)&&e.find("body differs")!=std::string::npos,"modified native frame admitted");++n;ui::frame_ok=true;
 bytes.data.erase(R+0x2d0);need(!ui::classify_origin(bytes,image,R,origin,e),"unreadable outer frame became Foreign success");++n;
 for(auto [ret,wanted]:{std::pair{uintptr_t{0x759c53},ui::Origin::NpcWindow},std::pair{uintptr_t{0x66de55},ui::Origin::BuildingWindow},std::pair{uintptr_t{0x59bc48},ui::Origin::DestroyCursor}}){bytes.data[R+0xa0]=image+ret;need(ui::classify_origin(bytes,image,R,origin,e)&&origin==wanted,"existing caller changed");++n;}
 for(auto source:{ui::Origin::MonsterWindow,ui::Origin::DestroyCursor,ui::Origin::SelectionWindow}){
  Services s;ui::Capture capture(s);uintptr_t object=20;ui::NativePropArray array{100,&object,1,1};need(capture.publish_disabled(&original,e)&&capture.set_enabled(true,e),e);
  auto report=capture.destroy_props(reinterpret_cast<void*>(10),&array,source);need(report.disposition==ui::Disposition::IntentCaptured&&s.monsters==1&&!s.calls&&!s.props&&!s.npcs,"Monster capture executed locally or used another typed family");++n;
 }
 {Services s;ui::Capture c(s);uintptr_t object=20;ui::NativePropArray a{100,&object,1,1};need(c.publish_disabled(&original,e)&&c.set_enabled(true,e),e);
  need(c.destroy_props(reinterpret_cast<void*>(10),&a,ui::Origin::NpcWindow).disposition==ui::Disposition::Rejected&&!s.calls&&!s.monsters,"NPC window origin admitted for Monster");++n;
  need(c.destroy_props(reinterpret_cast<void*>(10),&a,ui::Origin::Foreign).original_returned&&s.calls==1,"Foreign OriginalOnce changed");++n;
  ui::Replay replay{ui::ReplayPurpose::HostExecution,{action_id,snapshot_id,s.binding.canonical_record_key},s.binding};
  need(c.with_replay(replay,[&](std::string&){return c.destroy_props(reinterpret_cast<void*>(10),&a,ui::Origin::Foreign).original_returned;},e)&&s.calls==2&&c.status().replay_originals==1,e);++n;
  s.binding.uid=1;bool threw=false;
  try{c.with_replay(replay,[&](std::string&){return c.destroy_props(reinterpret_cast<void*>(10),&a,ui::Origin::Foreign).original_returned;},e);}
  catch(const std::runtime_error& cause){threw=std::string(cause.what()).find("binding differs")!=std::string::npos;}
  need(threw&&s.calls==2&&c.status().unknown,"private full UID mismatch reached original");++n;
 }
 bytes.data[R+0xa0]=image+0x82bbfb;
 need(ui::classify_origin(bytes,image,R,origin,e)&&origin==ui::Origin::ZoneWindow,"actual Zone Destroy caller not classified");++n;
 for(auto source:{ui::Origin::ZoneWindow,ui::Origin::DestroyCursor,ui::Origin::SelectionWindow}){
  Services service;service.binding.supported_monster=false;service.binding.supported_zone=true;
  ui::Capture capture(service);uintptr_t object=20;ui::NativePropArray array{100,&object,1,1};
  need(capture.publish_disabled(&original,e)&&capture.set_enabled(true,e),e);
  const auto report=capture.destroy_props(reinterpret_cast<void*>(10),&array,source);
  need(report.disposition==ui::Disposition::IntentCaptured&&service.zones==1&&!service.calls&&!service.monsters&&!service.props,"Zone Remove did not suppress into its exact typed family");++n;
 }
 {Services service;service.binding.supported_monster=false;service.binding.supported_zone=true;
  ui::Capture capture(service);uintptr_t object=20;ui::NativePropArray array{100,&object,1,1};
  need(capture.publish_disabled(&original,e)&&capture.set_enabled(true,e),e);
  need(capture.destroy_props(reinterpret_cast<void*>(10),&array,ui::Origin::MonsterWindow).disposition==ui::Disposition::Rejected&&!service.calls&&!service.zones,
       "Monster window admitted a Zone mutation");++n;
  ui::Replay replay{ui::ReplayPurpose::HostExecution,{action_id,snapshot_id,service.binding.canonical_record_key},service.binding};
  need(capture.with_replay(replay,[&](std::string&){return capture.destroy_props(reinterpret_cast<void*>(10),&array,ui::Origin::Foreign).original_returned;},e)&&service.calls==1&&capture.status().replay_originals==1,e);++n;
  service.binding.supported_monster=true;
  need(capture.destroy_props(reinterpret_cast<void*>(10),&array,ui::Origin::ZoneWindow).disposition==ui::Disposition::Rejected&&service.calls==1,
       "Ambiguous Zone/Monster class flags reached original");++n;
 }
 std::cout<<n<<" actual Remove caller/capture/replay cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
