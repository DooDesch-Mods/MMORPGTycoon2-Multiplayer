#pragma once
#include "mt2mp/game/normal_load_identity.h"

namespace mt2mp::game::root_building_load {
using Ticket = uint64_t;
using ArrayFn = void(*)(void*, void*, void*);
using ChildrenFn = int32_t(*)(void*);
using InlineFn = void(*)(void*, void*);
bool initialize(std::string& error);
bool confirm_hooks(uint32_t mask, std::string& error);
void* entrypoint(size_t index);
bool begin(const std::string& sha, const std::string& uuid, std::string& error);
Ticket before(void* array, void* reader, void* context) noexcept;
void children(uintptr_t caller, void* reader, int32_t count) noexcept;
void registered(uintptr_t caller, void* context, int32_t id, void* object) noexcept;
void after(Ticket ticket, void* array, void* reader, void* context) noexcept;
void original_failed(Ticket ticket, const std::string& cause) noexcept;
bool finalize(std::vector<normal_load_identity::Binding>& result, std::string& error);
void cancel(const std::string& cause);
#ifdef MT2MP_ROOT_LOAD_TEST
namespace testing {
using Typed = bool(*)(void*, std::string&);
using Lookup = bool(*)(void*, int32_t, void*&, std::string&);
using Readback = bool(*)(uint64_t, void*, native::BuildingState&, std::string&);
void backend(uintptr_t image, Typed type, Lookup lookup, Readback readback);
}
#endif
}
