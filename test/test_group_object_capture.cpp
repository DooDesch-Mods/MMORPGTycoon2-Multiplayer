#include "ui_remove/ui_capture.h"
#include "group_object_key.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace ui=mt2mp::action_session::ui_capture;
namespace {
const std::string snapshot="11111111-1111-4111-8111-111111111111",request="22222222-2222-4222-8222-222222222222";
const std::string saved=std::string(64,'a')+":world:building:1";
unsigned checks=0;
void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
struct Services final:ui::Services {
    ui::LocalBinding binding{0x100,0x200,3,4,5,snapshot,"group:"+saved,false,false,false,false,false,true};
    ui::Policy mode=ui::Policy::AuthorityRequired;
    unsigned policies=0,resolves=0,groups=0,normal=0,originals=0,rejections=0,authorizations=0;
    bool game_thread=true,authorize=true;
    bool on_game_thread()noexcept override{return game_thread;}
    ui::Policy policy(std::string& e)override{++policies;e.clear();return mode;}
    bool array(ui::NativePropArray*,ui::ArrayObservation& out,std::string& e)override{out={0x300,0x400,binding.object,1,1};e.clear();return true;}
    bool resolve(uintptr_t manager,uintptr_t object,ui::LocalBinding& out,std::string& e)override{
        ++resolves;if(manager!=binding.manager||object!=binding.object){e="fixture manager/object differs";return false;}out=binding;e.clear();return true;}
    bool fresh_action_uuid(std::string& out,std::string& e)override{out=request;e.clear();return true;}
    bool submit(const ui::Intent&,std::string& e)override{++normal;e.clear();return true;}
    bool submit_group(const ui::Intent& intent,std::string& e)override{++groups;e.clear();return intent.canonical_record_key==binding.canonical_record_key;}
    bool authorize_replay(const ui::Replay& replay,std::string& e)override{++authorizations;
        if(!authorize){e="fixture pending Group action differs";return false;}e.clear();return replay.binding==binding;}
    void original(ui::DestroyProps,void*,ui::NativePropArray*)override{++originals;}
    void log(const char*,const char*,const std::string&)noexcept override{}
    void show_rejection(const std::string&)noexcept override{++rejections;}
};
void original(void*,ui::NativePropArray*){}
}
int main()try{
    std::string e;Services service;ui::Capture capture(service);ui::NativePropArray array;
    need(capture.publish_disabled(&original,e)&&capture.set_enabled(true,e),e);
    auto source=[&](ui::Origin origin){return capture.destroy_props(reinterpret_cast<void*>(service.binding.manager),&array,origin);};
    for(const auto origin:{ui::Origin::DestroyCursor,ui::Origin::SelectionWindow}){
        need(source(origin).disposition==ui::Disposition::IntentCaptured&&service.normal==0&&service.originals==0,"Group source did not use its typed whole-graph request");}
    service.binding.canonical_record_key="group:action:"+request;
    need(source(ui::Origin::SelectionWindow).disposition==ui::Disposition::IntentCaptured&&service.groups==3,"new native Group action alias was rejected");
    need(source(ui::Origin::OtherWindow).disposition==ui::Disposition::Rejected,"unproved Group InfoWindow Destroy caller was admitted");
    service.binding.supported_normal_scenery=true;
    need(source(ui::Origin::SelectionWindow).disposition==ui::Disposition::Rejected,"Group and normal Scenery classification overlapped");service.binding.supported_normal_scenery=false;
    service.binding.canonical_record_key=saved;
    need(source(ui::Origin::SelectionWindow).disposition==ui::Disposition::Rejected,"Group classification accepted a normal prop key");
    service.binding.canonical_record_key="group:"+saved;service.mode=ui::Policy::Unavailable;
    const auto policies=service.policies,resolves=service.resolves;
    need(source(ui::Origin::Foreign).disposition==ui::Disposition::ForeignOriginal&&service.originals==1&&service.policies==policies&&service.resolves==resolves,
         "Foreign original TypeDestroy callback entered Group policy/classification");service.mode=ui::Policy::AuthorityRequired;
    ui::Replay replay{ui::ReplayPurpose::HostExecution,{request,snapshot,service.binding.canonical_record_key},service.binding};
    need(capture.with_replay(replay,[&](std::string&){return source(ui::Origin::Foreign).disposition==ui::Disposition::OwnReplayOriginal;},e)&&
         service.originals==2&&service.authorizations==1,e);
    service.authorize=false;bool unauthorized_entered=false;
    need(!capture.with_replay(replay,[&](std::string&){unauthorized_entered=true;return false;},e)&&!unauthorized_entered&&
         e=="fixture pending Group action differs"&&service.originals==2,"Group replay did not retain its authorization cause");
    for(const auto& key:{"group:"+saved,"group:action:"+request})need(mt2mp::game::group_object_key::valid(key),"valid Group key rejected");
    for(const auto& key:std::vector<std::string>{saved,"group:action:not-a-uuid","group:group:"+saved})need(!mt2mp::game::group_object_key::valid(key),"invalid Group key accepted");
    service.authorize=true;service.binding.canonical_record_key=saved;service.binding.supported_group=false;service.binding.supported_normal_scenery=true;
    need(source(ui::Origin::SelectionWindow).disposition==ui::Disposition::IntentCaptured&&service.normal==1&&service.groups==3,
         "normal Scenery entered the Group submission route");
    std::cout<<checks<<" Group source/classifier/replay cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
