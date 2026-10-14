#include "mt2mp/overlay/notification_overlay.h"
#include <imgui.h>
#include <algorithm>

namespace mt2mp::overlay {

NotificationOverlay& NotificationOverlay::instance() {
    static NotificationOverlay inst;
    return inst;
}

void NotificationOverlay::show(const std::string& text, NotificationType type, float duration_sec) {
    active_.push_back({text, type, duration_sec, 1.0f});
}

void NotificationOverlay::update(float dt) {
    for (auto& n : active_) {
        n.remaining -= dt;
        n.alpha = std::clamp(n.remaining, 0.0f, 1.0f);
    }
    active_.erase(
        std::remove_if(active_.begin(), active_.end(),
            [](const Notification& n) { return n.remaining <= 0.0f; }),
        active_.end());
}

void NotificationOverlay::render() {
    if (active_.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    float x = io.DisplaySize.x - 336;
    float y = 16;

    for (size_t i = 0; i < active_.size() && i < 5; ++i) {
        const auto& n = active_[i];

        // Status colors matching the game palette
        ImVec4 accent;
        ImU32 border_col;
        switch (n.type) {
            case NotificationType::Success:
                accent = ImVec4(0.46f, 0.77f, 0.50f, n.alpha);
                border_col = IM_COL32(118, 196, 129, (int)(n.alpha * 180));
                break;
            case NotificationType::Warning:
                accent = ImVec4(0.89f, 0.74f, 0.42f, n.alpha);
                border_col = IM_COL32(227, 188, 106, (int)(n.alpha * 180));
                break;
            case NotificationType::Error:
                accent = ImVec4(0.84f, 0.41f, 0.41f, n.alpha);
                border_col = IM_COL32(215, 105, 105, (int)(n.alpha * 180));
                break;
            default:
                accent = ImVec4(0.46f, 0.63f, 0.85f, n.alpha);
                border_col = IM_COL32(118, 162, 216, (int)(n.alpha * 180));
        }

        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::SetNextWindowSize(ImVec2(320, 0));
        ImGui::SetNextWindowBgAlpha(0.92f * n.alpha);

        char label[32];
        snprintf(label, sizeof(label), "##toast_%zu", i);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.5f);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(
            ((border_col >> 0) & 0xFF) / 255.0f,
            ((border_col >> 8) & 0xFF) / 255.0f,
            ((border_col >> 16) & 0xFF) / 255.0f,
            n.alpha * 0.7f));

        ImGui::Begin(label, nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

        // Status indicator dot + text
        ImGui::TextColored(accent, "●");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.88f, 0.85f, 0.76f, n.alpha), "%s", n.text.c_str());

        y += ImGui::GetWindowHeight() + 8.0f;

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    }
}

} // namespace mt2mp::overlay
