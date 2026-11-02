#pragma once
#include <cstdint>
#include <exception>
#include <string>

namespace mt2mp::game::management_service_graph {
// A failed native lock or unlock has unknown ownership. Never retry the lock
// after that point. A read failure still releases a successfully acquired lock.
template<class Calls,class Body>bool with_read_lock(Calls& calls,uintptr_t manager,bool& unknown,Body&& body,std::string& error){
    if(unknown){error="Service graph native lock ownership is Unknown";return false;}
    if(!calls.lock(manager,error)){unknown=true;if(error.empty())error="Service graph native read lock did not return";return false;}
    bool ok=false;std::string read_error;
    try{ok=body(read_error);}catch(const std::exception& ex){read_error=std::string("Service graph read exception: ")+ex.what();}
    catch(...){read_error="Service graph read nonstandard exception";}
    std::string unlock_error;
    if(!calls.unlock(manager,unlock_error)){unknown=true;error="Service graph native read unlock did not return";
        if(!unlock_error.empty())error+=": "+unlock_error;if(!read_error.empty())error+="; prior read: "+read_error;return false;}
    if(!ok){error=read_error.empty()?"Service graph read failed":read_error;return false;}
    error.clear();return true;
}
}
