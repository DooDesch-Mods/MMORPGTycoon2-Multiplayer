#pragma once
#include "tick_commands.h"
#include <memory>

namespace mt2mp::action_session {
bool observe_completed(Engine&,const Binding&,const Request&,const Ledger&,Evidence&,std::string&);
}
namespace mt2mp::action_session::tick_commands {
struct ExecutionReceipt {
    Assigned command;
    Outcome outcome=Outcome::Pending;
    bool native_attempted=false;
    std::shared_ptr<const Evidence> evidence;
    std::optional<game::time_session::Clock> clock_before,clock_after;
    std::string cause;
};
struct ClockPort {
    std::function<bool(game::time_session::Clock&,std::string&)> read;
    std::function<bool(float,std::string&)> set;
    // Read-only observation by the current held simulation owner.
    std::function<bool(game::time_session::Clock&,std::string&)> read_held;
};
// The native owner must admit the deterministic boundary and operation scopes.
// This does not change Session::Role or manufacture Host-result authority.
using AdmitExecution=std::function<bool(const Context&,const Binding&,const Assigned&,std::string&)>;
using PrepareSnapshot=std::function<bool(const SnapshotRequest&,std::string&)>;
class Executor {
public:
    Executor(Session&,Engine&,Context,Binding,ClockPort,AdmitExecution,uint64_t initial_order=0,PrepareSnapshot = {});
    Executor(const Executor&)=delete;
    Executor& operator=(const Executor&)=delete;
    Outcome execute(const Assigned&,ExecutionReceipt&,std::string&);
private:
    Session& session_;
    Engine& engine_;
    Context context_;
    Binding binding_;
    ClockPort clock_;
    AdmitExecution admit_;PrepareSnapshot prepare_snapshot_;
    std::thread::id thread_;
    uint64_t completed_=0;
    bool busy_=false,locked_=false;
    std::optional<ExecutionReceipt> last_;
    std::string failure_;
    bool current(std::string&);
    Outcome gameplay(const Request&,ExecutionReceipt&,std::string&);
    Outcome time(const TimeRequest&,ExecutionReceipt&,std::string&);
};
}
