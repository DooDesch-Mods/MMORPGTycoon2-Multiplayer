#include "mt2mp/hooks/hook_opengl.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"

#include <windows.h>
#include <gl/GL.h>
#include <cmath>
#include <cstring>

// GL constants
#define GL_MODELVIEW_MATRIX  0x0BA6
#define GL_PROJECTION_MATRIX 0x0BA7

using GlGetFloatvFn = void(*)(unsigned int pname, float* params);
using GlViewportFn = void(*)(int x, int y, int w, int h);
using WglGetProcAddressFn = void*(*)(const char* name);

namespace mt2mp::hooks {

static GlGetFloatvFn orig_glGetFloatv = nullptr;
static GlViewportFn orig_glViewport = nullptr;
static WglGetProcAddressFn orig_wglGetProcAddress = nullptr;

static CameraState g_camera;

// Hook for glUniformMatrix4fv (obtained via wglGetProcAddress)
using GlUniformMatrix4fvFn = void(*)(int location, int count, unsigned char transpose, const float* value);
static GlUniformMatrix4fvFn orig_glUniformMatrix4fv = nullptr;
static int g_uniform_call_count = 0;

// Heuristic: first large uniform matrix upload per frame is likely the view matrix,
// second is projection. Reset counter each frame in SwapWindow.
static void detour_glUniformMatrix4fv(int location, int count, unsigned char transpose, const float* value) {
    if (count == 1 && value) {
        g_uniform_call_count++;
        // Capture first two 4x4 matrix uniforms per frame as view/projection candidates
        if (g_uniform_call_count == 1) {
            memcpy(g_camera.modelview, value, 16 * sizeof(float));
            g_camera.valid = true;
        } else if (g_uniform_call_count == 2) {
            memcpy(g_camera.projection, value, 16 * sizeof(float));
        }
    }
    orig_glUniformMatrix4fv(location, count, transpose, value);
}

static void detour_glGetFloatv(unsigned int pname, float* params) {
    orig_glGetFloatv(pname, params);

    // Cache matrices when the game queries them
    if (params) {
        if (pname == GL_MODELVIEW_MATRIX) {
            memcpy(g_camera.modelview, params, 16 * sizeof(float));
            g_camera.valid = true;
        } else if (pname == GL_PROJECTION_MATRIX) {
            memcpy(g_camera.projection, params, 16 * sizeof(float));
        }
    }
}

static void detour_glViewport(int x, int y, int w, int h) {
    g_camera.viewport[0] = x;
    g_camera.viewport[1] = y;
    g_camera.viewport[2] = w;
    g_camera.viewport[3] = h;
    orig_glViewport(x, y, w, h);
}

static void* detour_wglGetProcAddress(const char* name) {
    void* result = orig_wglGetProcAddress(name);

    // Intercept glUniformMatrix4fv to capture shader matrix uploads
    if (result && name && strcmp(name, "glUniformMatrix4fv") == 0) {
        orig_glUniformMatrix4fv = reinterpret_cast<GlUniformMatrix4fvFn>(result);
        LOG_INFO("Intercepted wglGetProcAddress(\"glUniformMatrix4fv\") -> 0x{:X}",
            reinterpret_cast<uintptr_t>(result));
        return reinterpret_cast<void*>(&detour_glUniformMatrix4fv);
    }

    return result;
}

// Called each frame from SwapWindow hook to reset per-frame counters
void reset_frame_state() {
    g_uniform_call_count = 0;
}

const CameraState& get_camera_state() {
    return g_camera;
}

// Basic matrix inversion for 4x4 (used for unproject)
static bool invert_matrix(const float m[16], float inv[16]) {
    float det;
    inv[0] = m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] +
             m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4] = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] -
             m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8] = m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] +
             m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] -
              m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];

    det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (fabs(det) < 1e-10) return false;

    inv[1] = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] -
             m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5] = m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] +
             m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9] = -m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] -
             m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] = m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] +
              m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2] = m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] +
             m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6];
    inv[6] = -m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] -
             m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6];
    inv[10] = m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] +
              m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] -
              m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5];
    inv[3] = -m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] -
             m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6];
    inv[7] = m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] +
             m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] -
              m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5];
    inv[15] = m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] +
              m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5];

    float inv_det = 1.0f / det;
    for (int i = 0; i < 16; i++) inv[i] *= inv_det;
    return true;
}

bool screen_to_world(int screen_x, int screen_y, float& world_x, float& world_y, float& world_z) {
    if (!g_camera.valid || g_camera.viewport[2] == 0) return false;

    // Normalize to [-1, 1]
    float ndc_x = (2.0f * screen_x / g_camera.viewport[2]) - 1.0f;
    float ndc_y = 1.0f - (2.0f * screen_y / g_camera.viewport[3]); // flip Y

    // Compute inverse(projection * modelview)
    float mvp[16], inv_mvp[16];
    // MVP = projection * modelview (column-major multiply)
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            mvp[j*4+i] = 0;
            for (int k = 0; k < 4; k++) {
                mvp[j*4+i] += g_camera.projection[k*4+i] * g_camera.modelview[j*4+k];
            }
        }
    }

    if (!invert_matrix(mvp, inv_mvp)) return false;

    // Unproject NDC point at z=0 (near plane intersection)
    float in[4] = {ndc_x, ndc_y, 0.0f, 1.0f};
    float out[4];
    for (int i = 0; i < 4; i++) {
        out[i] = inv_mvp[i] * in[0] + inv_mvp[4+i] * in[1] +
                 inv_mvp[8+i] * in[2] + inv_mvp[12+i] * in[3];
    }

    if (fabs(out[3]) < 1e-10) return false;
    world_x = out[0] / out[3];
    world_y = out[1] / out[3];
    world_z = out[2] / out[3];
    return true;
}

bool install_opengl_hooks() {
    bool ok = true;
    ok &= create_hook_by_import("OPENGL32.dll", "glGetFloatv",
        reinterpret_cast<void*>(&detour_glGetFloatv), reinterpret_cast<void**>(&orig_glGetFloatv));
    ok &= create_hook_by_import("OPENGL32.dll", "glViewport",
        reinterpret_cast<void*>(&detour_glViewport), reinterpret_cast<void**>(&orig_glViewport));
    ok &= create_hook_by_import("OPENGL32.dll", "wglGetProcAddress",
        reinterpret_cast<void*>(&detour_wglGetProcAddress), reinterpret_cast<void**>(&orig_wglGetProcAddress));
    return ok;
}

} // namespace mt2mp::hooks
