#pragma once
#include "construction_types.h"
#include <cstddef>
namespace mt2mp::game::developer_staff::construction {
uint32_t reorder(uintptr_t image,uintptr_t manager,uintptr_t actor,struct Call*);
struct Call {
    bool attempted=false,returned=false,record_confirmed=false,accepted=false,cleanup_confirmed=false;
    uint32_t exception=0,cleanup_exception=0;
};
enum class Unary {OnMap,OffMap,UpdateQuadtree,UpdateBuildingColors,DestroyJob};
uint32_t unary(uintptr_t image,Unary,uintptr_t object,Call*);
uint32_t stop_loaded_movement(uintptr_t image,uintptr_t actor,Call*);
uint32_t membership(uintptr_t image,uintptr_t lounge,uintptr_t actor,bool outside,Call*);
uint32_t pose(uintptr_t image,uintptr_t actor,const ActorPose*,const std::array<float,3>*,Call*);
uint32_t shift(uintptr_t image,uintptr_t destination,uintptr_t source,size_t bytes,Call*);
uint32_t active_property(uintptr_t image,uintptr_t building,Call*);
uint32_t link_property(uintptr_t image,uintptr_t property,uintptr_t receiver,bool developer_property,
                       bool attach,void* context,Call*);
}
