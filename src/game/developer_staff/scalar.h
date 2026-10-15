#pragma once
#include <cstdint>
#include <functional>
#include <string>
namespace mt2mp::game::developer_staff::scalar {
enum class Field {State,Active};
using Read=std::function<bool(uintptr_t,void*,size_t,std::string&)>;
struct Receipt {
    bool constructed=false,record_confirmed=false,attempted=false,returned=false,accepted=false,cleanup_confirmed=false,readback=false;
    uint32_t exception=0,cleanup_exception=0;
};
const char* state_name(int32_t)noexcept;
// Caller binds this local actor to an ordered Client frame and its actual owned
// Mainthread/Origin/census. These scalar calls never run Tick or resolve objects.
bool apply(const Read&,uintptr_t image,uintptr_t actor,Field,int32_t value,Receipt&,std::string&);
uint32_t invoke(uintptr_t image,uintptr_t actor,Field,int32_t value,Receipt*);
}
