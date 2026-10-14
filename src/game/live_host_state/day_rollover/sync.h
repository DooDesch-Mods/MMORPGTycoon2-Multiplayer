#pragma once
#include "day.h"
#include <optional>
namespace mt2mp::game::live_host_state::day {
class Sync {
    Port& records_;live_host_state::Port& scalars_;Context context_;Binding binding_;
    Journal journal_;Apply apply_;std::optional<NativeHistory> make_before_;
    bool suspended_=false,locked_=false;
    std::string cause_;
public:
    Sync(Port& r,live_host_state::Port& s,const Context& c,const Binding& b):records_(r),scalars_(s),context_(c),binding_(b){}
    bool bootstrap(std::string&);
    bool capture(const Frame&,Target&,std::string&);
    bool actual_current(const Frame& lineage,Target&,std::string&);
    bool take(const Target&,ApplyReceipt&,std::string&);
    bool after_action(const action_session::Message&,std::string&);
    bool observe_make(bool entering,uint32_t original_seh,std::string&);
    bool suspend_snapshot(std::string&);
    bool rebind_snapshot(const Context&,const Binding&,std::string&);
    const Receipt& day_receipt()const{return apply_.receipt();}
    bool locked()const{return locked_||apply_.locked()||journal_.unknown;}
    bool suspended()const{return suspended_;}
    uint64_t sequence()const{return context_.role==action_session::Role::Host?journal_.current_sequence:(apply_.known().empty()?0:apply_.known().back().sequence);}
};
void bind_host_journal(Sync*);
bool observe_host_make(bool entering,uint32_t original_seh,std::string&);
}
