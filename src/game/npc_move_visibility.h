#pragma once
#include "npc_target_key.h"

#include <cstdint>
#include <string>

namespace mt2mp::game::npc_move_visibility {

template<class State>
bool target(const State& before,const std::string& key) {
    return before.type.empty()&&before.variant.empty()&&npc_target_key::valid(key);
}

struct Snapshot {
    uintptr_t character_actor=0;
    uintptr_t actor=0;
    uintptr_t skeleton=0;
    bool on_map=false;
    bool actor_visible=false;
};

struct Receipt {
    Snapshot before,after;
    unsigned original_calls=0;
};

template<class Read>
bool read(uintptr_t image,uintptr_t object,Read&& field,Snapshot& out,std::string& error) {
    uintptr_t ap=0,set_on_map=0;
    uint8_t on_map=2,visible=2;
    if(!field(object,0,ap,error))return false;
    if(ap!=image+0x1742980){
        error="NPC Move visibility target is not exact NPC class";return false;}
    if(!field(object,0x368,on_map,error)||
       !field(object,0x6f0,out.character_actor,error)||
       !field(ap,0x290,set_on_map,error))return false;
    if(!out.character_actor){error="NPC Move CharacterActor binding absent";return false;}
    if(!field(out.character_actor,0,out.actor,error))return false;
    if(!out.actor){error="NPC Move Actor binding absent";return false;}
    if(!field(out.actor,0x40,out.skeleton,error))return false;
    if(!out.skeleton){error="NPC Move Skeleton binding absent";return false;}
    if(!field(out.skeleton,0xe4,visible,error))return false;
    if(set_on_map!=image+0x3c2a00||on_map>1||visible>1){
        error="NPC Move SetOnMap slot or boolean state differs";
        return false;
    }
    out.on_map=on_map!=0;out.actor_visible=visible!=0;
    return true;
}

template<class Read>
bool read_spawn(uintptr_t image,uintptr_t object,Read&& field,Snapshot& out,
                const char* phase,std::string& error) {
    if(!read(image,object,field,out,error))return false;
    if(!out.on_map){
        error=std::string("NPC Move ")+phase+" target OnMap absent on_map="+
            std::to_string(out.on_map)+" actor_visible="+
            std::to_string(out.actor_visible);
        return false;
    }
    return true;
}

template<class Read,class Invoke>
bool restore(uintptr_t image,uintptr_t object,Read&& field,Invoke&& original,
             Receipt& receipt,std::string& error) {
    receipt={};
    if(!read(image,object,field,receipt.before,error))return false;
    if(receipt.before.on_map!=receipt.before.actor_visible){
        error="NPC Move OnMap and Actor visibility differ before restore";
        return false;
    }
    if(!receipt.before.on_map){
        ++receipt.original_calls;
        if(!original(object,error)){
            if(error.empty())error="NPC Move original SetOnMap failed without cause";
            return false;
        }
    }
    if(!read(image,object,field,receipt.after,error))return false;
    if(!receipt.after.on_map||!receipt.after.actor_visible||
       receipt.before.character_actor!=receipt.after.character_actor||
       receipt.before.actor!=receipt.after.actor||
       receipt.before.skeleton!=receipt.after.skeleton){
        error="NPC Move original SetOnMap did not restore stable visible Actor";
        return false;
    }
    error.clear();return true;
}

template<class Read,class Invoke,class Submit>
bool restore_then_submit(uintptr_t image,uintptr_t object,Read&& field,
                         Invoke&& original,Submit&& submit,Receipt& receipt,
                         std::string& error) {
    if(!restore(image,object,field,original,receipt,error))return false;
    if(!submit(error)){
        if(error.empty())error="NPC Move restored visibility but Controller submit failed without cause";
        return false;
    }
    return true;
}

}
