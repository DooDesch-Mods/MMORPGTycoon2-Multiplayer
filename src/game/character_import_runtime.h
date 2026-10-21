#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::npc_probe::monster_action_runtime::character_membership {
bool enable_import(std::string&);
// Returns true only when the existing SetCharacterType hook consumed the exact
// scoped presentation tail of the one original image import.
bool import_presentation(void* view,void* selected,uintptr_t caller,bool& handled,std::string&);
nlohmann::json import_status();
}
