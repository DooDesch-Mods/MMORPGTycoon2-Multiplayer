#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::monster_zone_shared_ui {
using json=nlohmann::json;
template<class ReadBinding>
bool admit(const json& controller,bool local_test,bool pending_ui,const std::string& snapshot,
           action_session::Binding& binding,ReadBinding&& read,std::string& e){
    if(!local_test||!controller.value("initialized",false)||!controller.value("started",false)||
       controller.value("locked",true)||controller.value("pending",true)||pending_ui){
        e="Zone shared UI requires idle admitted Controller";return false;
    }
    return read(snapshot,binding,e);
}
template<class ReadBinding>
bool current(const std::string& expected_snapshot,const action_session::Binding& expected,
             const std::string& actual_snapshot,ReadBinding&& read,std::string& e){
    if(actual_snapshot!=expected_snapshot){e="Zone shared UI common snapshot changed";return false;}
    action_session::Binding actual;if(!read(expected_snapshot,actual,e))return false;
    if(actual!=expected){e="Zone shared UI current Controller binding changed";return false;}return true;
}
inline bool captured(const json& before,const json& after,const json& receipt,bool pending,
                     const std::string& original,std::string& e){
    if(before!=after){e="Zone suppressed UI stroke changed native world/ledger/RNG/Undo/Release";return false;}
    if(!receipt.is_object()||!receipt.value("captured",false)||receipt.value("submitted",true)||!pending){
        e="Zone hooked original UI did not retain one effectless pending capture: "+original;return false;
    }return true;
}
}
