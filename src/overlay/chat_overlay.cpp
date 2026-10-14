#include "mt2mp/overlay/chat_overlay.h"
#include "mt2mp/net/chat_runtime.h"

#include <imgui.h>

namespace mt2mp::overlay {

ChatOverlay& ChatOverlay::instance() {
    static ChatOverlay inst;
    return inst;
}

void ChatOverlay::toggle() {
    focus_.toggle();
}

bool ChatOverlay::is_visible() const { return focus_.visible(); }
bool ChatOverlay::is_typing() const { return focus_.typing(); }

ChatUiSnapshot ChatOverlay::ui_snapshot() const {
    std::lock_guard lock(ui_mutex_);
    ChatUiSnapshot result = observed_;
    result.visible = focus_.visible();
    result.typing = focus_.typing();
    if (!result.visible) result.input_active = false;
    return result;
}

void ChatOverlay::publish_ui(bool rendered_visible, bool input_active) {
    std::lock_guard lock(ui_mutex_);
    observed_.rendered_visible = rendered_visible;
    observed_.input_active = input_active;
    observed_.draft = input_buf_;
    ++observed_.rendered_frames;
}

void ChatOverlay::render() {
    if (!focus_.visible()) { publish_ui(false, false); return; }
    const auto snapshot = net::chat::Runtime::instance().snapshot();
    ImGui::SetNextWindowSize(ImVec2(420, 310), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(80, 100), ImGuiCond_FirstUseEver);
    bool open = true;
    if (!ImGui::Begin("Koop-Chat###mt2mp_chat", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) focus_.close();
        publish_ui(false, false);
        return;
    }
    if (!snapshot.connected) ImGui::TextUnformatted("Keine verbundene Gegenstelle");
    else ImGui::TextUnformatted("Verbunden");
    ImGui::BeginChild("##chat_messages", ImVec2(0, -62), true);
    for (const auto& entry : snapshot.entries) {
        const std::string sender = entry.origin == snapshot.local_peer ? "Du" :
            entry.origin == 1 ? "Host" : "Mitspieler " + std::to_string(entry.origin);
        const char* state = "";
        if (entry.origin == snapshot.local_peer) {
            if (entry.delivery == net::chat::Delivery::Queued) state = " - Senden";
            else if (entry.delivery == net::chat::Delivery::Confirmed) state = " - Empfangen";
            else if (entry.delivery == net::chat::Delivery::Failed) state = " - Fehlgeschlagen";
        }
        ImGui::TextWrapped("%s: %s%s", sender.c_str(), entry.text.c_str(), state);
    }
    if (snapshot.entries.size() != last_count_) ImGui::SetScrollHereY(1.0f);
    last_count_ = snapshot.entries.size();
    ImGui::EndChild();
    const bool requested_focus = focus_.take_focus_request();
    if (requested_focus) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(-86);
    const bool enter = ImGui::InputText("##chat_text", input_buf_, sizeof(input_buf_), ImGuiInputTextFlags_EnterReturnsTrue);
    const bool input_active = ImGui::IsItemActive();
    focus_.set_input_active(input_active || requested_focus);
    ImGui::SameLine();
    const bool send_clicked = ImGui::Button("Senden");
    if ((enter || send_clicked) && snapshot.connected) {
        std::string error;
        if (net::chat::Runtime::instance().submit(input_buf_, error)) {
            input_buf_[0] = '\0'; input_error_.clear(); focus_.request_input_focus();
        } else input_error_ = std::move(error);
    }
    if (!input_error_.empty()) ImGui::TextColored(ImVec4(0.84f, 0.41f, 0.41f, 1.0f), "%s", input_error_.c_str());
    ImGui::End();
    if (!open) focus_.close();
    publish_ui(open && focus_.visible(), open && input_active && focus_.visible());
}

} // namespace mt2mp::overlay
