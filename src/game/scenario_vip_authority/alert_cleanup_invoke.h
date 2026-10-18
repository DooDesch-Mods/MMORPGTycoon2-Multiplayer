#pragma once
#include "alert_cleanup_hook.h"
#include <Windows.h>
namespace mt2mp::game::scenario_vip_authority::client_alert::cleanup_hook {
using Original=void(*)(uintptr_t);
inline void invoke(Original original,const Callbacks& callbacks,uintptr_t mode){
    callbacks.before(mode);
    __try{original(mode);}
    __finally{callbacks.after(mode,AbnormalTermination()!=FALSE);}
}
}
