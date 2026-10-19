#pragma once
#include "dungeon_instance_lifecycle.h"
#include "typed_action/action_session.h"

namespace mt2mp::game::dungeon_instance_native {
bool initialize(uintptr_t image,std::string& error);
bool healthy(std::string& error);
bool bind(const action_session::Context&,const action_session::Binding&,
          const dungeon_instance_lifecycle::journal::Origin&,std::string& error);
bool reset(std::string& error);
// Root brackets the actual synchronous Engine action. Nested Reset/Force calls
// belong to one original effect and receive only one operation ordinal.
bool begin_action(const std::string& operation,std::string& error);
bool end_action(const std::string& operation,bool original_returned,std::string& error);
// Natural operations acquire the supplied Frame operation UUID only when their
// retained native observations are drained on Mainthread. Root owns the journal.
bool drain(const std::string& natural_operation,std::vector<dungeon_instance_lifecycle::Event>&,
           std::vector<dungeon_instance_lifecycle::Pin>& final_pools,std::string& error);
// Root calls this only after its existing action/Frame retention permits release.
bool release_action(const std::string& operation,std::string& error);
bool adopt_counters(const std::vector<std::pair<std::string,int32_t>>&,
                    dungeon_instance_lifecycle::Receipt&,std::string& error);
class NativePort final:public dungeon_instance_lifecycle::Port {
public:
    bool current(const std::string&,dungeon_instance_lifecycle::Pin&,std::string&)override;
    bool observed(const dungeon_instance_lifecycle::Event&,bool&,std::string&)override;
    bool invoke(const dungeon_instance_lifecycle::Pin&,const dungeon_instance_lifecycle::Event&,uintptr_t,
                dungeon_instance_lifecycle::Receipt&,std::string&)override;
};
}
