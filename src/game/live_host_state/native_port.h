#pragma once
#include "state.h"
namespace mt2mp::game::live_host_state {
// Building refunds and history cash run inside one native edit; Path refunds run inside two nested edits.
// Frame stores and action entry/exit still require an idle world.
inline bool native_activity(int32_t saves,int32_t edits,int32_t expected_edits,std::string& e) {
    if((expected_edits!=0&&expected_edits!=1&&expected_edits!=2)||saves!=0||edits!=expected_edits) {
        e="live native save/edit scope differs saves="+std::to_string(saves)+
            " edits="+std::to_string(edits)+" required_saves=0 required_edits="+std::to_string(expected_edits);
        return false;
    }
    e.clear();return true;
}
inline bool publication_activity(int32_t saves,int32_t edits,bool& waiting,std::string& e) {
    waiting=false;
    if(saves>0&&edits==0){
        waiting=true;e="info: Host publication waits for native save saves="+std::to_string(saves)+" edits=0";return true;
    }
    return native_activity(saves,edits,0,e);
}
struct EconomicBoundary {
    virtual ~EconomicBoundary()=default;
    // Root reads its actual installed dispatch records and current world scope.
    virtual bool read_current_fence(const Pin&,std::string&)=0;
};
class NativePort final:public Port {
    Context context_;Binding binding_;EconomicBoundary* boundary_=nullptr;
public:
    NativePort(const Context& c,const Binding& b,EconomicBoundary* boundary):context_(c),binding_(b),boundary_(boundary){}
    bool publication_ready(uint64_t,bool&,std::string&)override;
    bool read(Pin&,std::string&)override;
    bool store(const Pin&,size_t,int32_t,int32_t,std::string&)override;
    bool rebind_snapshot(const Context&,const Binding&,std::string&);
    bool current_owners(const Pin&,std::string&,int32_t expected_edits=0);
};
bool read_time_authority(const Context&,uint64_t& revision,std::string&);
bool capture_host(const Context&,const Binding&,uint64_t revision,uint64_t action_revision,uint64_t time_revision,Frame&,std::string&);
}
