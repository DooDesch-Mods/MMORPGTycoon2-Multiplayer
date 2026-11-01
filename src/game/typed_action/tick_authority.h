#pragma once
#include "tick_commands.h"
namespace mt2mp::action_session::tick_commands::authority {
// Authorization exists only while Queue invokes its current admitted command.
// Keep actual Host/Client roles and original native world ownership unchanged.
bool active();
bool matches(const Context&,const Binding&,const Request&,std::string&);
bool matches(const Binding&,const Request&,std::string&);
bool current_time(TimeRequest&,std::string&);
bool current_snapshot(SnapshotRequest&,std::string&);
Outcome run(Queue&,Session&,Engine&,const Binding&,const Assigned&,
            const std::function<Outcome(std::string&)>&,std::string&);
}
