#pragma once
#include <array>
#include <cstdint>
#include <exception>
#include <string>
namespace mt2mp::game::ability_apply_lease {
struct Slots {
    uintptr_t outer=0,draft=0,live=0,proxy=0,owner=0;
    std::array<uint8_t,2> dirty{};
    bool operator==(const Slots&)const=default;
};
// Every attempted borrow is restored, including a partially failed write. Cleanup
// runs before the caller observes gameplay because native destruction touches Release.
template<class Calls>bool run(Calls& calls,std::string& error){
    bool ok=false;
    try {ok=calls.prepare(error)&&calls.borrow(error)&&calls.invoke(error);}
    catch(const std::exception& why){error=std::string("Ability Apply boundary: ")+why.what();}
    catch(...){error="Ability Apply non-standard boundary exception";}
    std::string cleanup;
    bool clean=false;
    try {clean=calls.cleanup(cleanup);}
    catch(const std::exception& why){cleanup=std::string("Ability Apply cleanup exception: ")+why.what();}
    catch(...){cleanup="Ability Apply non-standard cleanup exception";}
    if(!clean){if(!error.empty())error+="; ";error+=cleanup.empty()?"Ability Apply cleanup failed without cause":cleanup;}
    return ok&&clean;
}
}
