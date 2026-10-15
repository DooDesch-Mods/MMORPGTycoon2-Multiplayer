#pragma once
#include "developer_staff/read.h"
namespace mt2mp::game::developer_staff::retirement {
struct Details {
    uintptr_t job=0,carried=0,quadtree=0,current=0,destination=0;
    int32_t current_index=0,destination_index=0;
    uint8_t moving=0;
    float tick_delay=0;
    bool operator==(const Details&)const=default;
};
enum class Route {IndoorIdle,Arriving};
struct Plan {size_t slot=0;int32_t id=0;Route route=Route::IndoorIdle;};
bool prepare(const Capture&,const Details&,size_t slot,int32_t id,Plan&,std::string&);
bool effect(const Capture&,const Details&,const Capture&,const Details&,const Plan&,std::string&);
struct Call {
    bool constructed=false,record_confirmed=false,attempted=false,arrival_returned=false;
    bool property_returned=false,property_accepted=false,cleanup_attempted=false,cleanup_confirmed=false;
    uint32_t exception=0,cleanup_exception=0;
};
uint32_t invoke_original(uintptr_t image,uintptr_t developer,Route,Call*);
// Isolated native fixture only. The shared Controller must remain uninitialized.
bool run_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool observe_fixture(nlohmann::json&,std::string&);
}
