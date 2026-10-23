#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::tutorial_startingpoint_fixture {
template<class Context,class Binding,class Intent>
bool queued_project_admission(bool present,uint64_t before,uint64_t captures,const Context& actual_context,
    const Binding& actual_binding,const Intent& actual_intent,const std::string& id,
    const Context& context,const Binding& binding,const Intent& intent,std::string& error){
    if(!present||before==UINT64_MAX||captures!=before+1||id.size()!=36){
        error="General project UI acknowledgement requires exactly one fresh queued intent";return false;
    }
    if(actual_context!=context||actual_binding!=binding||actual_intent!=intent){
        error="General project UI acknowledgement Context/Binding/intent differs";return false;
    }
    error.clear();return true;
}
}
