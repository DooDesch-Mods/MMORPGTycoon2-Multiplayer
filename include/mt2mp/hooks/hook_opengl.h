#pragma once

#include <cstdint>

namespace mt2mp::hooks {

struct CameraState {
    float modelview[16] = {};
    float projection[16] = {};
    int viewport[4] = {};
    bool valid = false;
};

const CameraState& get_camera_state();

// Convert screen pixel coords to a world-space ray direction
bool screen_to_world(int screen_x, int screen_y, float& world_x, float& world_y, float& world_z);

bool install_opengl_hooks();

} // namespace mt2mp::hooks
