#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>
namespace mt2mp::game::tutorial_startingpoint_fixture {
class UpdateScope {
    bool outer_;
public:
    explicit UpdateScope(void* game) noexcept;
    ~UpdateScope();
    UpdateScope(const UpdateScope&)=delete;
    UpdateScope& operator=(const UpdateScope&)=delete;
};
struct Request {
    std::string token, origin_uuid, operation;
    uint64_t epoch=0, generation=0;
    nlohmann::json expected_development;
};
// Fixed isolated Host proof. It does not advance the tutorial or replicate a peer.
bool run(const Request&,nlohmann::json& receipt,std::string& error);
}
