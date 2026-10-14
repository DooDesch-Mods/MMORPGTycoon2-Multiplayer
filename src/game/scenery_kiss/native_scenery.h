#pragma once
#include "scenery_state.h"
#include "placement_kiss/placement_adapter.h"

namespace mt2mp::scenery_kiss {
using Update=void(*)(void*,float);
struct PrivateScenery {
    void* receiver=nullptr;void* options=nullptr;
    alignas(8) std::array<unsigned char,32> directory{};
    uint32_t stage=0,exception=0,cleanup_exception=0;
    bool options_constructed=false,update_attempted=false,update_returned=false,cursor_dtor_attempted=false,cursor_dtor_returned=false,options_dtor_attempted=false,options_dtor_returned=false;
    bool directory_ctor_attempted=false,directory_ctor_returned=false,directory_dispose_attempted=false,directory_dispose_returned=false;
};
class NativeScenery {
    uintptr_t image_=0;uint32_t thread_=0;Update original_=nullptr;
public:
    bool bind_before_hooks(uintptr_t image,uint32_t actual_main_thread,std::string&);
    bool publish_typed_original(Update,std::string&);
    bool consume_captured_intent(native::Memory&,Catalog&,uintptr_t actual_root_shard,uintptr_t receiver,const State&,uint32_t& exception,std::string&)const;
    // category/hash come from the exact native catalog roundtrip, never a wire UID.
    bool prepare(const State&,int32_t category,int32_t variant_hash,void* actual_root_shard,PrivateScenery&,std::string&)const;
    bool update_once(PrivateScenery&,std::string&)const;
    bool destroy_once(PrivateScenery&,std::string&)const;
};
}
