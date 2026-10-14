#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

void check(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
bool time_initialization_attempted=false,time_initialized=false;
std::string time_initialization_error;
std::vector<std::string> calls;
bool observer_ok=true,hud_ok=true,calendar_ok=true,facade_ok=true,facade_forbidden=false;
std::string expected_reason="verified snapshot replaces world";
void called(const char* operation,const std::string& reason){check(reason==expected_reason,"retirement reason changed");calls.emplace_back(operation);}
namespace game {
namespace time_input_diagnostics {void suspend(const std::string& reason){called("input",reason);}}
namespace node_observer {
bool suspend_if_prepared(const std::string& reason,std::string& error){called("node",reason);if(!observer_ok)error="original node observer failure";return observer_ok;}
}
namespace hud_time_runtime {
bool suspend(const std::string& reason,std::string& error){called("hud",reason);if(!hud_ok)error="original HUD failure";return hud_ok;}
}
namespace hud_calendar_runtime {
bool suspend(const std::string& reason,std::string& error){called("calendar",reason);if(!calendar_ok)error="original calendar failure";return calendar_ok;}
}
namespace time_integration {
struct Facade {
    bool suspend(const std::string& reason,std::string& error){called("facade",reason);if(!facade_ok)error="original facade retirement failure";else error.clear();return facade_ok;}
};
Facade& instance(){check(!facade_forbidden,"cold/failed initialization touched a Time facade");calls.emplace_back("instance");static Facade facade;return facade;}
}
}
#include "cold_time_retire_body.inc"
void reset(bool attempted=false,bool initialized=false){
    time_initialization_attempted=attempted;time_initialized=initialized;time_initialization_error.clear();calls.clear();
    observer_ok=hud_ok=calendar_ok=facade_ok=true;facade_forbidden=!initialized;
}
void sequence(std::initializer_list<const char*> expected){
    std::vector<std::string> values;for(const auto* value:expected)values.emplace_back(value);
    check(calls==values,"retirement call sequence/count differs");
}
int main(){
    std::string error="stale caller error";
    reset();check(suspend_time(expected_reason,error)&&error.empty(),"cold retirement should be a clean no-op after diagnostics");sequence({"input","node"});
    reset();observer_ok=false;
    check(!suspend_time(expected_reason,error)&&error=="original node observer failure","cold no-op swallowed observer failure");sequence({"input","node"});
    reset(true,false);time_initialization_error="original Time initialization rejection";
    check(!suspend_time(expected_reason,error)&&error=="time_integration_not_initialized: original Time initialization rejection","attempted initialization failure was waived or cause changed");sequence({"input","node"});
    reset(true,false);
    check(!suspend_time(expected_reason,error)&&error=="time_integration_not_initialized: ","empty recorded initialization cause was treated as cold");sequence({"input","node"});
    reset(true,false);observer_ok=false;time_initialization_error="previous initialization failure";
    check(!suspend_time(expected_reason,error)&&error=="original node observer failure","initialization guard hid observer failure");sequence({"input","node"});
    reset(true,true);error="stale caller error";
    check(suspend_time(expected_reason,error)&&error.empty(),"active retirement failed");sequence({"input","node","hud","calendar","instance","facade"});
    reset(false,true);
    check(suspend_time(expected_reason,error),"existing owner was mistaken for cold state");sequence({"input","node","hud","calendar","instance","facade"});
    reset(true,true);observer_ok=false;
    check(!suspend_time(expected_reason,error)&&error=="original node observer failure","active observer failure changed");sequence({"input","node"});
    reset(true,true);hud_ok=false;
    check(!suspend_time(expected_reason,error)&&error=="original HUD failure","active HUD failure changed");sequence({"input","node","hud"});
    reset(true,true);calendar_ok=false;
    check(!suspend_time(expected_reason,error)&&error=="original calendar failure","active calendar failure changed");sequence({"input","node","hud","calendar"});
    reset(true,true);facade_ok=false;
    check(!suspend_time(expected_reason,error)&&error=="original facade retirement failure","active facade failure changed");sequence({"input","node","hud","calendar","instance","facade"});
    std::cout<<"PASS actual suspend_time body: cold diagnostics-only retirement, attempted failure retained, existing owner and active retirement order/causes unchanged\n";
}
