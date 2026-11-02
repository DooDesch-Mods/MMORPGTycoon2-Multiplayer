#include "typed_action/action_session.h"
#include "typed_action/ordered_building_family.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
bool scope=true,admission=true;unsigned checks=0,admission_calls=0,routed=0;
namespace mt2mp::action_session {
namespace tick_commands::authority {
bool active(){return true;}
bool matches(const Binding&,const Request&,std::string& e){if(!scope)e="foreign authority";return scope;}
}
bool network_history(const Request&){return false;}
}
namespace ui_move_runtime {
namespace selection_move {bool history_intent(const action::Binding&,const action::Request&,action::SelectionMoveIntent&,std::string&){return false;}}
namespace duplicate_edit {bool history_targets(const action::Binding&,const action::Request&,std::map<uint64_t,std::string>&,std::string&){return false;}}
namespace group_edit {bool history_admitted(const action::Binding&,const action::Request&,std::string& e){++admission_calls;if(!admission)e="source rejected";return admission;}}
}
struct Engine {
    struct Source{action::Request request;};std::map<std::string,Source> buildings_,operations_;
    bool live_state_=false,live_port_=false,live_result_=false;
    action::Outcome execute_action_native(const action::Binding&,const action::Request&,std::string&){++routed;return action::Outcome::Confirmed;}
    action::Outcome execute(const action::Binding& binding,const action::Request& request,std::string& error){
        #include "ordered_path_route_actual.inc"
        return action::Outcome::RejectedNoEffect;
    }
};
void need(bool b){++checks;if(!b)throw std::runtime_error("ordered Group family route mismatch");}
int main()try{
    Engine engine;action::Binding binding{3,4};action::Request request;std::string e;
    request.action=action::Action::EditGroup;need(engine.execute(binding,request,e)==action::Outcome::Confirmed&&routed==1&&!admission_calls);
    for(auto direction:{action::Action::UndoBuilding,action::Action::RedoBuilding}){
        request.action=direction;request.target_action_uuid="source";request.canonical_record_key="groups:source";
        admission=true;need(engine.execute(binding,request,e)==action::Outcome::Confirmed);
        admission=false;need(engine.execute(binding,request,e)==action::Outcome::RejectedNoEffect);
        request.canonical_record_key="groups:foreign";need(engine.execute(binding,request,e)==action::Outcome::RejectedNoEffect);
    }
    request.action=action::Action::MoveGroup;need(engine.execute(binding,request,e)==action::Outcome::RejectedNoEffect);
    request.action=action::Action::RemoveGroup;need(engine.execute(binding,request,e)==action::Outcome::RejectedNoEffect);
    scope=false;request.action=action::Action::EditGroup;need(engine.execute(binding,request,e)==action::Outcome::RejectedNoEffect);
    std::cout<<checks<<" actual ordered Group family router cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
