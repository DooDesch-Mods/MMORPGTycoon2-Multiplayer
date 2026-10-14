#include "session_provider.h"
#include "typed_action/sidecar_adapter.h"
#include "typed_action/steam_action_adapter.h"
#include "mt2mp/core/config.h"
namespace mt2mp::action_session {
Session& selected_session(){
    static LocalSession local;static SteamSession steam;
    return Config::instance().local_test?static_cast<Session&>(local):static_cast<Session&>(steam);
}
}
