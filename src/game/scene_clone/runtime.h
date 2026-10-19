#pragma once
#include "state.h"
#include "../dungeon_scene_identity_runtime.h"
#include "../typed_action/action_session.h"
#include <memory>
namespace mt2mp::game::scene_clone {
class Runtime {
    struct State;
    std::unique_ptr<State> state_;
public:
    Runtime();
    ~Runtime();
    Runtime(const Runtime&)=delete;
    Runtime& operator=(const Runtime&)=delete;
    // Seed once after the completed native world bind. Rebinding the same
    // context does not replace the last confirmed raw roster.
    bool bind(const action_session::Context&,const action_session::Binding&,std::string&);
    // Client: before original lifecycle batch. Host: before capture. Neither
    // call advances the retained baseline or invokes a canonical Gizmo census.
    bool prepare(const action_session::Context&,const action_session::Binding&,std::string&);
    // Host passes null authority and a fresh operation UUID for the whole
    // ordered batch. Client passes that exact Host Mapping after native batch.
    // Registry-only publication occurs after all local native previews pass.
    bool confirm(const action_session::Context&,const action_session::Binding&,
        const std::string& operation,const Mapping* authority,Mapping& out,
        dungeon_scene_identity_runtime::Census& admitted,bool& attempted,std::string&);
    void reset();
};
}
