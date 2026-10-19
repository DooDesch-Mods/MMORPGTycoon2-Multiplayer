#pragma once
#include "typed_action/logo_action.h"
#include "subscriber_lifecycle/record_decode.h"
namespace mt2mp::game::logo_native {
struct Owned {uintptr_t pointer=0;bool constructed=false,adopted=false,unknown=false;};
bool read(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t logo,action_session::LogoState&,nlohmann::json& records,std::string&);
bool create(uintptr_t image,subscriber_lifecycle::Memory&,const action_session::LogoIntent&,Owned&,std::string&);
bool discard(uintptr_t image,Owned&,std::string&);
}
