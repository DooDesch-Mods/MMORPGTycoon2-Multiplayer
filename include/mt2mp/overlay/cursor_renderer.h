#pragma once

#include <cstdint>

namespace mt2mp::overlay {

class CursorRenderer {
public:
    static CursorRenderer& instance();

    // Render all remote player cursors
    void render();

    // World-to-screen projection (needs camera matrices)
    bool world_to_screen(float wx, float wy, float wz,
                         float& sx, float& sy) const;

    // Set camera matrices (extracted from game or GL state)
    void set_matrices(const float* view_4x4, const float* proj_4x4,
                      int viewport_w, int viewport_h);

private:
    CursorRenderer() = default;
    float view_[16] = {};
    float proj_[16] = {};
    int vp_w_ = 0, vp_h_ = 0;
    bool matrices_valid_ = false;
};

} // namespace mt2mp::overlay
