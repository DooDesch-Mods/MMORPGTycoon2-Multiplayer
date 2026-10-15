#include "tutorial_replication/startingpoint_creation_native.h"
#include <type_traits>
namespace creation=mt2mp::game::tutorial_replication::startingpoint::creation;
static_assert(std::is_same_v<decltype(&creation::call_purchase_for_free),DWORD(*)(uintptr_t,uintptr_t)noexcept>);
static_assert(std::is_same_v<decltype(&creation::call_set_current_project),DWORD(*)(uintptr_t,uintptr_t,uintptr_t)noexcept>);
// Compile the exact adapters without invoking any game address.
auto purchase_declaration=&creation::call_purchase_for_free;
auto project_declaration=&creation::call_set_current_project;
