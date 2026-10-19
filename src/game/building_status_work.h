#pragma once
#include "typed_action/action_session.h"
namespace mt2mp::game::building_status_work {
struct Readback {
    uint32_t bugs=0,jobs=0,linked=0,developers=0;
    std::string bugs_sha256,expected_bugs_sha256,pool_sha256;
    std::vector<std::array<uintptr_t,3>> children;
    nlohmann::json bugs_record=nlohmann::json::object(),pool_record=nlohmann::json::object();
};
bool read(const action_session::Context&,const action_session::Binding&,uintptr_t image,uint32_t thread,
          uintptr_t world,uintptr_t map,uintptr_t building,uintptr_t collection,const std::string& key,int32_t target,Readback&,std::string&);
}
