#include "runtime.h"
#include "invoke.h"
#include "fingerprints.h"
#include "mt2mp/core/hash.h"
#include "mt2mp/core/logger.h"
#include "mt2mp/hooks/hook_engine.h"
#include <array>
#include <limits>
#include <vector>

namespace mt2mp::game::render_pool_guard {
namespace {
std::array<void*, 9> originals{};
uintptr_t prepared_image = 0;
bool started = false;
std::string failure;

template<size_t Slot> void unary(void* self) {
    invoke(reinterpret_cast<Unary>(originals[Slot]), self);
}
template<size_t Slot> void binary(void* self, void* argument) {
    invoke(reinterpret_cast<Binary>(originals[Slot]), self, argument);
}
bool fail(std::string& error, const std::string& cause) {
    failure = cause;
    error = cause;
    return false;
}
}
bool initialize(uintptr_t image, size_t image_size, std::string& error) {
    if (started) {
        if (prepared_image == image && failure.empty()) { error.clear(); return true; }
        return fail(error, failure.empty() ? "render pool guard module changed" : failure);
    }
    started = true;
    if (!image || !image_size || image_size > std::numeric_limits<uintptr_t>::max() - image)
        return fail(error, "render pool guard image bounds invalid");
    // Validate every enclosing body before MinHook changes any entry bytes.
    for (const auto& body : original_bodies) {
        if (body.rva >= image_size || body.size > image_size - body.rva)
            return fail(error, "render pool guard body outside image RVA=" + std::to_string(body.rva));
        std::vector<unsigned char> bytes(body.size);
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(image + body.rva), bytes.data(), bytes.size(), &copied) || copied != bytes.size())
            return fail(error, "render pool guard body read failed RVA=" + std::to_string(body.rva) + " Win32=" + std::to_string(GetLastError()));
        if (mt2mp::sha256(bytes) != body.sha256)
            return fail(error, "render pool guard original body differs RVA=" + std::to_string(body.rva));
    }
    if (!InitializeCriticalSectionEx(&pool_lock, 0, 0))
        return fail(error, "render pool guard lock initialization failed Win32=" + std::to_string(GetLastError()));
    const std::array<void*, 9> detours = {
        reinterpret_cast<void*>(&unary<0>), reinterpret_cast<void*>(&unary<1>),
        reinterpret_cast<void*>(&binary<2>), reinterpret_cast<void*>(&binary<3>),
        reinterpret_cast<void*>(&binary<4>), reinterpret_cast<void*>(&binary<5>),
        reinterpret_cast<void*>(&binary<6>), reinterpret_cast<void*>(&binary<7>),
        reinterpret_cast<void*>(&unary<8>)};
    for (size_t i = 0; i < original_bodies.size(); ++i) {
        if (!hooks::create_hook(reinterpret_cast<void*>(image + original_bodies[i].rva), detours[i], &originals[i]) || !originals[i])
            return fail(error, "render pool guard hook creation failed RVA=" + std::to_string(original_bodies[i].rva));
    }
    prepared_image = image;
    error.clear();
    LOG_INFO("[RenderPool] Prepared all nine original pool-access hooks with one recursive lock");
    return true;
}
}
