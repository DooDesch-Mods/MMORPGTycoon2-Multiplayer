#pragma once
#include "initial_release/release_entry.h"
#include <vector>

namespace mt2mp::game::startingpoint_adoption {
struct Listener {uintptr_t address=0,address_point=0,handler=0;};
struct Admission {
    uintptr_t release=0;
    uint8_t events_enabled=0,repeatable=0;
    int32_t modifiers=0,item_feature=0;
    uint64_t art_pack_bytes=0;
    std::vector<Listener> listeners;
};
// Caller supplies the catalogue-owned Tech from tutorial_development's reciprocal
// owner read. Use its watched Memory and full NativeOpen before/after transaction.
// This reads prerequisites only. Success does not authorize a Client mutation.
bool read_admission(initial_release::Memory&,uintptr_t image,uintptr_t state,
                    uintptr_t catalogue_tech,Admission&,std::string&);
}
