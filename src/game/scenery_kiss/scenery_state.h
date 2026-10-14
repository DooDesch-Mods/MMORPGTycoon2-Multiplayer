#pragma once
#include "typed_action/action_session.h"
#include "object_identity/remove_runtime/copy/selection_witness.h"

namespace mt2mp::scenery_kiss {
namespace native=build_actions::private_native;
using State=action_session::BuildingState;
struct Catalog {
    virtual ~Catalog()=default;
    virtual bool identify(int32_t category,int32_t variant_hash,std::string& directory,std::string& filename,std::string&)=0;
};
struct Binding {
    uintptr_t object=0;uint64_t uid=0,epoch=0,generation=0;uintptr_t world=0,shard=0,region=0;
    bool operator==(const Binding&)const=default;
};
struct Root {
    virtual ~Root()=default;
    virtual bool bind(uint64_t uid,Binding&,std::string&)=0;
};
bool read_state(native::Memory&,Catalog&,Root&,uintptr_t image,uint64_t uid,State&,std::string&);
bool valid_state(const State&,std::string&);
enum class Capture {Forward,Single,Rejected};
// The installed owner's entry handles Mainthread/session/foreign routing before this reader.
Capture read_single_intent(native::Memory&,Catalog&,uintptr_t image,uintptr_t actual_root_shard,uintptr_t receiver,State&,std::string&);
}
