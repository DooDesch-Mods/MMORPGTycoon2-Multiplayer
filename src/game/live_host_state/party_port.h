#pragma once
#include "state.h"
#include "subscriber_lifecycle/reference_map.h"

namespace mt2mp::game::live_host_state {
struct PartyReceipt {bool attempted=false,confirmed=false,unknown=false;uint32_t calls=0,stores=0;std::string cause;};
struct PartyPort:subscriber_lifecycle::first_append::PartyReferences {
    virtual bool resolve_held_dungeon_destination(uintptr_t,std::string& key,bool& matched,std::string& e,std::string* scene=nullptr){
        if(scene)scene->clear();
        key.clear();matched=false;e.clear();return true;
    }
    virtual bool bind(const Context&,const Binding&,uint64_t common_epoch,std::string&)=0;
    virtual bool suspend(std::string&)=0;
    virtual bool begin_dungeon_building(const Context&,const Binding&,const action_session::Request&,std::string& e){
        e="Party provider has no Dungeon Building action owner";return false;
    }
    virtual bool finish_dungeon_building(const Context&,const Binding&,const action_session::Request&,
        const scene_clone::Mapping*,scene_clone::Mapping&,bool& attempted,std::string& e,uintptr_t=0){
        attempted=false;e="Party provider has no Dungeon Building action owner";return false;
    }
    virtual bool cancel_dungeon_building(const Context&,const Binding&,const action_session::Request&,std::string& e){
        e="Party provider has no Dungeon Building preparation owner";return false;
    }
    virtual bool prepare_capture(const Context&,const Binding&,uint64_t revision,std::string&)=0;
    virtual bool capture(const Context&,const Binding&,const Frame&,party_frame::State&,std::string&)=0;
    virtual bool prepare_apply(const Context&,const Binding&,const Frame&,PartyReceipt&,std::string&)=0;
    virtual bool apply(const Context&,const Binding&,const Frame&,const std::vector<nlohmann::json>& graphs,
                       nlohmann::json& receipt,std::string&)=0;
    virtual bool sent(const Frame&,std::string&)=0;
    virtual bool acknowledge(uint64_t peer,uint64_t revision,std::string&)=0;
    virtual bool committed(const Frame&,std::string&)=0;
    virtual bool admit(const Frame&,const Frame* previous,std::string&)=0;
    virtual nlohmann::json status()const=0;
};
}
