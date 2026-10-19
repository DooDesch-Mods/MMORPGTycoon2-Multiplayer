#pragma once
#include "gm_policy_state.h"
namespace mt2mp::game::gm_policy::native {
struct Receipt {bool stored=false,notified=false;uint32_t exception=0;};
Receipt change(uintptr_t image,uintptr_t policy,const Intent&)noexcept;
}
