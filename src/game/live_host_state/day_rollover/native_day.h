#pragma once
#include "day.h"
#include "live_host_state/economic_fence.h"
namespace mt2mp::game::subscriber_lifecycle_append {struct QuestEditBatch;}
namespace mt2mp::game::live_host_state::day {
bool prepare_original_guards(std::string&);
class NativePort final:public Port {
    Context context_;Binding binding_;EconomicBoundary& fence_;
    enum class Resource {Allocated,Constructed,Initialized,Published,Unknown};
    std::map<uintptr_t,Resource> resources_;
    std::vector<uintptr_t> retained_arrays_;

    bool current(const Pin&,std::string&);
    bool read_once(NativeHistory&,std::string&,bool share_metadata=false,subscriber_lifecycle_append::QuestEditBatch* held=nullptr);
    bool record_at(uintptr_t,uintptr_t image,Record&,std::string&);
public:
    NativePort(const Context& c,const Binding& b,EconomicBoundary& f):context_(c),binding_(b),fence_(f){}
    bool rebind_snapshot(const Context&,const Binding&,std::string&)override;
    bool read(NativeHistory&,std::string&)override;
    bool read_quest_held(NativeHistory&,subscriber_lifecycle_append::QuestEditBatch&,std::string&);
    bool write_closed(const NativeHistory&,uintptr_t,const Record&,const Record&,std::string&)override;
    bool create(const Record&,uintptr_t&,std::string&)override;
    bool append(const NativeHistory&,uintptr_t,const Record&,std::string&)override;
    bool commit_current(const NativeHistory&,uintptr_t,const Frame&,std::string&)override;
    bool prune(const NativeHistory&,size_t,std::string&)override;
};
}
