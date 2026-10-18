#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::scenario_vip_authority::client_alert::cleanup_hook {
struct Callbacks {
    void(*before)(uintptr_t mode) noexcept=nullptr;
    void(*after)(uintptr_t mode,bool abnormal) noexcept=nullptr;
};
bool initialize(uintptr_t image,const Callbacks&,std::string&);
bool verify(std::string&);
}
