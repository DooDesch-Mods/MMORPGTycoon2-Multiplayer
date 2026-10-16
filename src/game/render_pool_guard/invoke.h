#pragma once
#include <Windows.h>

namespace mt2mp::game::render_pool_guard {
using Unary = void (*)(void*);
using Binary = void (*)(void*, void*);
extern CRITICAL_SECTION pool_lock;
void invoke(Unary original, void* self);
void invoke(Binary original, void* self, void* argument);
}
