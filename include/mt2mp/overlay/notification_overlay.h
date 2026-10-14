#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace mt2mp::overlay {

enum class NotificationType { Info, Warning, Error, Success };

class NotificationOverlay {
public:
    static NotificationOverlay& instance();

    void show(const std::string& text, NotificationType type = NotificationType::Info,
              float duration_sec = 5.0f);
    void render();
    void update(float dt);

private:
    NotificationOverlay() = default;

    struct Notification {
        std::string text;
        NotificationType type;
        float remaining;
        float alpha;
    };
    std::vector<Notification> active_;
};

} // namespace mt2mp::overlay
