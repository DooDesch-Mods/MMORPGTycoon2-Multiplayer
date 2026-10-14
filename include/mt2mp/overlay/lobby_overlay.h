#pragma once

#include "mt2mp/net/steam_matchmaking.h"
#include <vector>

namespace mt2mp::overlay {

class LobbyOverlay {
public:
    static LobbyOverlay& instance();

    void render();
    bool is_visible() const;
    void toggle();
    void set_visible(bool v);

private:
    LobbyOverlay() = default;
    bool visible_ = false;
    std::vector<net::LobbyInfo> lobby_list_;
    bool refreshing_ = false;

    void render_main_menu();
    void render_session_view();
    void render_create_tab();
    void render_browse_tab();
    void render_session_tab();
};

} // namespace mt2mp::overlay
