#include "ui_remove/native_contract.h"
#include <cstring>
#include <iostream>
#include <map>
#include <vector>
namespace ui=mt2mp::action_session::ui_capture;
struct Memory final:ui::Memory {
    std::map<uintptr_t,uint8_t> bytes;
    void put(uintptr_t address,uintptr_t value){const auto* source=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=source[i];}
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)override{auto* target=static_cast<uint8_t*>(out);
        for(size_t i=0;i<count;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){error="original native caller frame read failure";return false;}target[i]=found->second;}return true;}
    bool region(uintptr_t,ui::Region&,std::string& error)override{error="unexpected native region query";return false;}
};
unsigned native_calls=0;
void original(void*,ui::NativePropArray*){++native_calls;}
struct Services final:ui::Services {
    unsigned policies=0,rejections=0;std::vector<std::string> logs;
    bool on_game_thread()noexcept override{return true;}
    ui::Policy policy(std::string& error)override{++policies;error="Controller is pending Action87";return ui::Policy::Unavailable;}
    bool array(ui::NativePropArray*,ui::ArrayObservation&,std::string& error)override{error="unexpected UI capture array read";return false;}
    bool resolve(uintptr_t,uintptr_t,ui::LocalBinding&,std::string& error)override{error="unexpected UI target capture";return false;}
    bool fresh_action_uuid(std::string&,std::string& error)override{error="unexpected new UI request";return false;}
    bool submit(const ui::Intent&,std::string& error)override{error="unexpected second native user request";return false;}
    bool authorize_replay(const ui::Replay&,std::string& error)override{error="unexpected UI replay permission";return false;}
    void original(ui::DestroyProps function,void* manager,ui::NativePropArray* array)override{function(manager,array);}
    void log(const char*,const char* operation,const std::string& error)noexcept override{logs.push_back(std::string(operation)+": "+error);}
    void show_rejection(const std::string&)noexcept override{++rejections;}
};
int main()try{
    unsigned checks=0;const auto check=[&](bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);};
    constexpr uintptr_t image=0x140000000,slot=0x20000000;std::string error;Memory memory;ui::Origin origin;
    memory.put(slot,image+0x2d55a8);memory.put(slot+0xa0,image+0x3bcb34);memory.put(slot+0x2d0,image+0x3b9e71);
    check(ui::classify_origin(memory,image,slot,origin,error)&&origin==ui::Origin::Foreign,"Native CharacterTypeDestroyed callback became a Monster InfoWindow user click");
    Services services;ui::Capture capture(services);check(capture.publish_disabled(&original,error)&&capture.set_enabled(true,error),error);
    const auto result=capture.destroy_props(nullptr,nullptr,origin);
    check(result.disposition==ui::Disposition::ForeignOriginal&&result.original_attempted&&result.original_returned&&native_calls==1,"Original nested Monster destruction did not return exactly once");
    check(!services.policies&&!services.rejections&&!capture.status().captured&&!capture.status().unknown,"Native Character consequence was blocked by pending Controller/UI admission");
    memory.bytes.erase(slot+0xa0);check(!ui::classify_origin(memory,image,slot,origin,error)&&error=="original native caller frame read failure","Unreadable native frame fabricated a classification or lost its cause");
    memory.put(slot,image+0x2d55a8);memory.put(slot+0xa0,image+0x759c53);
    check(ui::classify_origin(memory,image,slot,origin,error)&&origin==ui::Origin::NpcWindow,"Actual native NPC user click lost its existing capture domain");
    memory.put(slot,image+0x2d55a8);memory.put(slot+0xa0,image+0x6cb61c);
    check(ui::classify_origin(memory,image,slot,origin,error)&&origin==ui::Origin::DistrictWindow,"Actual District InfoWindow caller lost its exact capture domain");
    memory.put(slot+0xa0,image+0x6cb61d);
    check(ui::classify_origin(memory,image,slot,origin,error)&&origin==ui::Origin::Foreign,"Adjacent unproved District return address was admitted");
    std::cout<<checks<<" actual Character callback caller classification and original-once pending-Controller comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
