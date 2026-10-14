#include "mt2mp/overlay/cursor_renderer.h"
#include "mt2mp/game/player_info.h"
#include "mt2mp/net/steam_networking.h"
#include "mt2mp/core/logger.h"

#include <imgui.h>
#include <cstring>
#include <cmath>
#include <unordered_map>

namespace mt2mp::overlay {

// Interpolated cursor state per player
struct SmoothedCursor {
    float display_x = 0;
    float display_y = 0;
    float target_x = 0;
    float target_y = 0;
    bool initialized = false;
};

static std::unordered_map<uint64_t, SmoothedCursor> s_cursors;

CursorRenderer& CursorRenderer::instance() {
    static CursorRenderer inst;
    return inst;
}

void CursorRenderer::set_matrices(const float* view_4x4, const float* proj_4x4,
                                   int viewport_w, int viewport_h) {
    if (view_4x4) std::memcpy(view_, view_4x4, 16 * sizeof(float));
    if (proj_4x4) std::memcpy(proj_, proj_4x4, 16 * sizeof(float));
    vp_w_ = viewport_w;
    vp_h_ = viewport_h;
    matrices_valid_ = (view_4x4 && proj_4x4 && viewport_w > 0);
}

bool CursorRenderer::world_to_screen(float wx, float wy, float wz,
                                      float& sx, float& sy) const {
    sx = wx;
    sy = wz;
    return true;
}

// Smooth lerp with adaptive speed
static float smooth_lerp(float current, float target, float speed, float dt) {
    float diff = target - current;
    if (std::abs(diff) < 0.5f) return target; // snap when close
    float t = 1.0f - std::exp(-speed * dt);
    return current + diff * t;
}

void CursorRenderer::render() {
    auto* draw_list = ImGui::GetBackgroundDrawList();
    auto local_id = net::SteamNetworking::instance().local_steam_id();
    const auto& players = game::PlayerRegistry::instance().all();
    float dt = ImGui::GetIO().DeltaTime;

    for (const auto& [steam_id, player] : players) {
        if (!player.connected) continue;
        if (steam_id == local_id) continue;

        float target_x = player.cursor_world_x;
        float target_y = player.cursor_world_z;

        if (target_x <= 0 && target_y <= 0) continue;

        // Get or create smoothed cursor
        auto& sc = s_cursors[steam_id];
        sc.target_x = target_x;
        sc.target_y = target_y;

        if (!sc.initialized) {
            sc.display_x = target_x;
            sc.display_y = target_y;
            sc.initialized = true;
        } else {
            // Exponential smoothing -- feels natural, not laggy
            // Speed 15 = responsive but smooth (~66ms effective delay)
            sc.display_x = smooth_lerp(sc.display_x, sc.target_x, 15.0f, dt);
            sc.display_y = smooth_lerp(sc.display_y, sc.target_y, 15.0f, dt);
        }

        float sx = sc.display_x;
        float sy = sc.display_y;

        ImU32 color = player.color;

        // Draw cursor arrow (classic pointer shape)
        ImVec2 p1(sx, sy - 14);
        ImVec2 p2(sx - 9, sy + 5);
        ImVec2 p3(sx + 9, sy + 5);
        draw_list->AddTriangleFilled(p1, p2, p3, color);
        draw_list->AddTriangle(p1, p2, p3, IM_COL32(0, 0, 0, 220), 2.0f);

        // Player name label
        const char* name = player.display_name.empty()
            ? "Player" : player.display_name.c_str();
        ImVec2 text_pos(sx + 14, sy - 6);

        auto text_size = ImGui::CalcTextSize(name);
        draw_list->AddRectFilled(
            ImVec2(text_pos.x - 3, text_pos.y - 2),
            ImVec2(text_pos.x + text_size.x + 3, text_pos.y + text_size.y + 2),
            IM_COL32(20, 27, 39, 200), 4.0f);
        draw_list->AddText(text_pos, color, name);
    }

    // Cleanup stale cursors
    for (auto it = s_cursors.begin(); it != s_cursors.end(); ) {
        if (players.find(it->first) == players.end()) {
            it = s_cursors.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace mt2mp::overlay
