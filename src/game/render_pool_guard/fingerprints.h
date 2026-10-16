#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mt2mp::game::render_pool_guard {
struct Fingerprint { uintptr_t rva; size_t size; const char* sha256; };
inline constexpr std::array<Fingerprint, 9> original_bodies = {{
    {0x9ab8f0, 288, "e2d8f267fc964adfd6c9508f2ba6de91b118b59c5b625b023d72be7eb83af186"},
    {0x9abca0, 368, "3c0ab20d17c89bd3a9f9042d3a5e965165080e3ea52d9a6937b679eb0d370556"},
    {0x9ad1a0, 368, "2ab2fdaf997827ffe988e452e1c79df52f6197a4a9ebe13d4448738ef6ad6939"},
    {0x9ad400, 368, "1830dda48b885b499fd5e7470ad692a59e09a94d5c9c1341b32ca27e0ae4dc5b"},
    {0x9ad570, 368, "bc07ba360bd4dea2176321979e5aa699dab736f8812f689936a5b436f89cceeb"},
    {0x9ad7d0, 368, "57f45635a697c21a58ce4765a72a3f0f694e3fe3385fdbb10d3d0210fa6ab5e9"},
    {0x9ad940, 368, "7c2386cfbbb2f7a75d0c8d413b520ace8771f2b0f05aa06d4467ab38a7201075"},
    {0x9af0d0, 384, "f412c3e33ab5b15a581279d0163f7ec9fa4d9968b52c4e34f0e0454e5e8a507c"},
    {0x9af500, 144, "3ba9e71580f3233f9be99ad990856d6c07db6908637fff623dcb31e53c4efd78"},
}};
}
