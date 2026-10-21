#pragma once
#include "typed_action/action_session.h"

namespace mt2mp::game::world_zone_runtime {
struct Current {
    std::string key,scene_key;
    uintptr_t object=0,root=0,region=0,manager=0;
    uint64_t uid=0;
    int32_t type=-1;
    bool operator==(const Current&)const=default;
};
// Read all four native arrays from the current admitted world. Saved identities
// come from the owned original serializer; this read cannot admit new members.
bool read_current(const action_session::Binding&,const std::string& snapshot,
    std::vector<Current>&,std::string& error);
// Validate one current owner pool without reading every world's Zone values.
bool resolve_current(const action_session::Binding&,const std::string& snapshot,
    uintptr_t object,uint64_t full_uid,Current&,std::string& error);
}
