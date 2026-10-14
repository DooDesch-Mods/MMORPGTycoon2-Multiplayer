#pragma once
#include "npc_target_key.h"

namespace npc_remove_replay_binding {
template<class Replay,class LocalBinding,class Authorize,class FindOperation,class FreshTarget>
bool resolve_private(const Replay& replay,uintptr_t manager,uintptr_t object,
                     LocalBinding& out,std::string& error,Authorize&& authorize,
                     FindOperation&& find_operation,FreshTarget&& fresh_target) {
    out={};
    if(!authorize(replay,error))return false;
    const auto* operation=find_operation(replay.intent.action_uuid);
    if(!operation||operation->request.action_uuid!=replay.intent.action_uuid||
       operation->request.snapshot_uuid!=replay.intent.snapshot_uuid||
       operation->request.canonical_record_key!=replay.intent.canonical_record_key||
       !mt2mp::game::npc_target_key::request(replay.intent.canonical_record_key,operation->request.target_action_uuid)){
        error="NPC Remove private replay operation or Place key differs";return false;
    }
    if(!fresh_target(*operation,out,error))return false;
    if(out!=replay.binding||out.manager!=manager||out.object!=object){
        out={};error="NPC Remove private replay fresh target binding differs";return false;
    }
    error.clear();return true;
}
}
