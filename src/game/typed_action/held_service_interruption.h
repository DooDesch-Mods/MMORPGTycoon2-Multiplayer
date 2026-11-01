#pragma once
#include <stdexcept>
#include <string>
namespace mt2mp::action_session::tick_commands {
struct HeldMembershipInterruption:std::runtime_error {using std::runtime_error::runtime_error;};
template<class Local>bool held_membership_interruption(bool pending,Local local,std::string& cause){
    if(!pending)return false;
    std::string local_error;if(!local(local_error)){cause+="; recovery local proof rejected: "+local_error;return false;}
    return true;
}
}
