#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace mt2mp::game::render_pool_guard {
bool initialize(uintptr_t image, size_t image_size, std::string& error);
}
