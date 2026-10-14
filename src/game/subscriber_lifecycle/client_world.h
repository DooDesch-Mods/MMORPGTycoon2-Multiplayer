#pragma once
#include "cohort.h"
#include "record_graph.h"
#include "native_open_runtime.h"
#include "mt2mp/game/native_engine.h"
namespace mt2mp::subscriber_lifecycle::first_append {
struct ClientWorld {
    game::building_mixed_identity::opened_file::CurrentWorld origin;
    uintptr_t image=0,manager=0,task=0,tick=0,update=0;
    uint32_t thread=0;
    Census census;
};
bool read_client_world(Memory&,const std::string& baseline_sha,const std::string& native_origin,
                       bool held,ClientWorld&,std::string&);
bool same_client_owner(const ClientWorld&,const ClientWorld&);
bool read_cohort(Memory&,const ClientWorld&,Census&,std::string&,bool require_partition=true);
}
