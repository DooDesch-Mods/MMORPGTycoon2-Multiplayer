#pragma once
#include "release_entry.h"

namespace mt2mp::game::initial_release {
struct StartingpointRelease {
    int32_t change_count=0,feature_count=0;
    uint32_t mutation_counter=0;
};
bool read_research_release(Memory&,uintptr_t image,uintptr_t game_state,
                           uintptr_t tech,const std::string& key,StartingpointRelease&,std::string&);
// The caller supplies a catalogue-owned startingpoint Tech and a watched Memory
// transaction. It must recheck all read bytes and full NativeOpen lifetime before
// publishing this sample. No ordered Release State or wire payload is changed.
bool read_startingpoint_release(Memory&,uintptr_t image,uintptr_t game_state,
                               uintptr_t startingpoint,StartingpointRelease&,std::string&);
}
