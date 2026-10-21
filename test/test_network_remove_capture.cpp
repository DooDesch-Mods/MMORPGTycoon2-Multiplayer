#include "ui_remove/ui_capture.h"
#include "ui_remove/native_contract.h"
#include "network_object_identity.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace ui=mt2mp::action_session::ui_capture;
namespace identity=mt2mp::game::network_object_identity;
int checks=0;void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
const std::string id="11111111-1111-4111-8111-111111111111",action="22222222-2222-4222-8222-222222222222";
struct Bytes:ui::Memory {std::map<uintptr_t,uintptr_t> values;bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{
    const auto i=values.find(p);if(i==values.end()||n!=8){e="exact frame unreadable";return false;}std::memcpy(out,&i->second,n);return true;}
    bool region(uintptr_t,ui::Region&,std::string&)override{return false;}};
struct Services:ui::Services {
    ui::LocalBinding binding{10,20,701,2,8,id,identity::birth(id,identity::Kind::Node),false,false,false,false,true};
    unsigned calls=0,submitted=0,wrong=0;bool authorizes=true;
    bool on_game_thread()noexcept override{return true;}ui::Policy policy(std::string&)override{return ui::Policy::AuthorityRequired;}
    bool array(ui::NativePropArray* p,ui::ArrayObservation& out,std::string&)override{out={p->addresspoint,uintptr_t(p->data),p->count?*p->data:0,p->count,p->capacity};return true;}
    bool resolve(uintptr_t,uintptr_t,ui::LocalBinding& out,std::string&)override{out=binding;return true;}
    bool fresh_action_uuid(std::string& out,std::string&)override{out=action;return true;}
    bool submit(const ui::Intent&,std::string&)override{++wrong;return false;}
    bool submit_network(const ui::Intent& intent,std::string&)override{need(intent.canonical_record_key==binding.canonical_record_key,"canonical target changed");++submitted;return true;}
    bool authorize_replay(const ui::Replay&,std::string& e)override{if(!authorizes)e="exact private authority rejected";return authorizes;}
    void original(ui::DestroyProps,void*,ui::NativePropArray*)override{++calls;}
    void log(const char*,const char*,const std::string&)noexcept override{}
    void show_rejection(const std::string&)noexcept override{}
};
void original(void*,ui::NativePropArray*){}
namespace scene_collection_fixture {
struct SceneServices final:Services {
    bool handled=true,failed=false,standalone=false;unsigned collections=0;
    ui::Policy policy(std::string&)override{return standalone?ui::Policy::Standalone:ui::Policy::AuthorityRequired;}
    bool scene_collection(uintptr_t manager,ui::NativePropArray* array,bool& captured,std::string& error)override{
        ++collections;need(manager==10&&array&&array->count==2,"Scene capture changed exact original collection");
        captured=handled;if(failed){error="exact Scene roster changed";return false;}return true;
    }
};
void cases(){
    for(unsigned scenario=0;scenario<6;++scenario){SceneServices services;ui::Capture capture(services);std::string error;
        uintptr_t targets[2]{20,30};ui::NativePropArray array{100,targets,2,2};
        need(capture.publish_disabled(&original,error)&&capture.set_enabled(true,error),error);
        services.failed=scenario==1;services.handled=scenario!=2;services.standalone=scenario==3;
        const auto origin=scenario==4?ui::Origin::Foreign:scenario==5?ui::Origin::NpcWindow:ui::Origin::SelectionWindow;
        const auto result=capture.destroy_props(reinterpret_cast<void*>(10),&array,origin);
        if(scenario==0)need(result.disposition==ui::Disposition::IntentCaptured&&capture.status().captured==1&&services.collections==1&&!services.calls&&!services.submitted,"Scene collection ran an original or individual action before Host ordering");
        if(scenario==1)need(result.disposition==ui::Disposition::Rejected&&capture.status().cause.find("exact Scene roster changed")!=std::string::npos&&!services.calls&&!services.submitted,"Scene failed roster lost its cause or reached original");
        if(scenario==2)need(result.disposition==ui::Disposition::Rejected&&!services.calls&&!services.submitted,"unhandled collection bypassed original one-prop guard");
        if(scenario==3||scenario==4)need(result.original_returned&&services.calls==1&&!services.collections,"standalone or foreign original entered Scene authority capture");
        if(scenario==5)need(result.disposition==ui::Disposition::Rejected&&!services.collections&&!services.calls,"another native caller family entered Scene collection capture");
    }
}
}
int main()try{scene_collection_fixture::cases();
    constexpr uintptr_t image=0x10000000,stack=0x30000000;std::string e;Bytes bytes;ui::Origin origin;
    for(auto pc:{0x42d8a4u,0x43411cu,0x74f588u}){bytes.values={{stack,image+0x2d55a8},{stack+0xa0,image+pc}};
        need(ui::classify_origin(bytes,image,stack,origin,e)&&origin==ui::Origin::NetworkWindow,"actual Node/Pipe/NetworkInfoWindow return chain missed");}
    bytes.values[stack+0xa0]=image+0x42d8a5;need(ui::classify_origin(bytes,image,stack,origin,e)&&origin==ui::Origin::Foreign,"adjacent unknown caller admitted");
    bytes.values.erase(stack+0xa0);need(!ui::classify_origin(bytes,image,stack,origin,e),"unreadable caller silently admitted");
    for(auto source:{ui::Origin::NetworkWindow,ui::Origin::DestroyCursor,ui::Origin::SelectionWindow}){Services s;ui::Capture capture(s);uintptr_t target=20;ui::NativePropArray array{100,&target,1,1};
        need(capture.publish_disabled(&original,e)&&capture.set_enabled(true,e),e);
        need(capture.destroy_props(reinterpret_cast<void*>(10),&array,source).disposition==ui::Disposition::IntentCaptured&&s.submitted==1&&!s.calls&&!s.wrong,"normal Network input applied before ordering or relabelled as scenery");}
    Services s;ui::Capture capture(s);uintptr_t target=20;ui::NativePropArray array{100,&target,1,1};need(capture.publish_disabled(&original,e)&&capture.set_enabled(true,e),e);
    need(capture.destroy_props(reinterpret_cast<void*>(10),&array,ui::Origin::NpcWindow).disposition==ui::Disposition::Rejected&&!s.calls&&!s.submitted,"wrong target family origin admitted");
    ui::Replay replay{ui::ReplayPurpose::HostExecution,{action,id,s.binding.canonical_record_key},s.binding};
    need(capture.with_replay(replay,[&](std::string&){return capture.destroy_props(reinterpret_cast<void*>(10),&array,ui::Origin::Foreign).original_returned;},e)&&s.calls==1&&capture.status().replay_originals==1,e);
    s.authorizes=false;need(!capture.with_replay(replay,[&](std::string&){++s.wrong;return true;},e)&&s.calls==1&&!s.wrong,"rejected authority executed callback");
    s.authorizes=true;++s.binding.uid;bool threw=false;
    try{capture.with_replay(replay,[&](std::string&){return capture.destroy_props(reinterpret_cast<void*>(10),&array,ui::Origin::Foreign).original_returned;},e);}catch(const std::runtime_error& cause){threw=std::string(cause.what()).find("binding differs")!=std::string::npos;}
    need(threw&&s.calls==1&&capture.status().unknown,"changed local full UID reached original");
    std::cout<<checks<<" actual Network caller/capture/private replay assertions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
