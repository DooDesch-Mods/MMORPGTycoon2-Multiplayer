#include "invoke.h"

namespace mt2mp::game::render_pool_guard {
// Native worker destruction and main-thread buffer reuse share one vector.
// Keep the lock for the process lifetime, including native teardown callbacks.
CRITICAL_SECTION pool_lock{};
void invoke(Unary original, void* self) {
    EnterCriticalSection(&pool_lock);
    __try { original(self); }
    __finally { LeaveCriticalSection(&pool_lock); }
}
void invoke(Binary original, void* self, void* argument) {
    EnterCriticalSection(&pool_lock);
    __try { original(self, argument); }
    __finally { LeaveCriticalSection(&pool_lock); }
}
}
