#pragma once
#include "visual.h"
namespace mt2mp::game::toon_weapon_visual {
struct Observation {
    Plan plan;
    bool actor_present=false,attachment_present=false,matching=false,matrix_observed=false,matrix_matching=false;
    uintptr_t instance=0,group=0,model=0,transform=0;
    int32_t actual_bone=-1,actual_attack=13,actual_model_id=-1;
    uint8_t visible=0,skeleton_visible=0;
    std::array<float,10> actual_trs{};
    std::array<float,4> color{},skeleton_color{};
    std::array<float,16> matrix{},bone_matrix{};
    Call matrix_call;
    std::string matrix_cause;
};
bool read_scene(placement_ui_probe::Memory&,Plan&,std::string&);
bool read_observation(placement_ui_probe::Memory&,Plan,Observation&,std::string&);
bool inspect(uintptr_t toon,Observation&,std::string&);
nlohmann::json encode_observation(const Observation&);
}
