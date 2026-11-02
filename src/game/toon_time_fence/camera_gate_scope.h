#pragma once
#include <cstdint>
#include <string>

namespace mt2mp::game::toon_time_fence::camera_gate {
inline constexpr uintptr_t full_update_rva=0x3ea480;
struct State {uintptr_t root=0,receiver=0;bool failed=false;std::string cause;};
inline thread_local State state;
class Step {
    State previous_;
    bool admitted_=false;
public:
    explicit Step(uintptr_t root):previous_(state){
        if(root&&!state.root&&!state.receiver){state={root,0,false,{}};admitted_=true;}
    }
    ~Step(){state=previous_;}
    bool admitted()const{return admitted_;}
    bool failed()const{return state.failed;}
    const std::string& cause()const{return state.cause;}
};
class Entry {
    uintptr_t previous_=0;
public:
    explicit Entry(uintptr_t receiver):previous_(state.receiver){state.receiver=receiver;}
    ~Entry(){state.receiver=previous_;}
};
inline bool selected(uintptr_t receiver){return state.root&&receiver&&state.receiver==receiver;}
}
