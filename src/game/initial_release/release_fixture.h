#pragma once
#include "vendor/nlohmann/json.hpp"
#include <string>
namespace mt2mp::game::initial_release {
class FixtureUpdateScope {
    bool outer_=false;
public:
    explicit FixtureUpdateScope(void* game)noexcept;
    ~FixtureUpdateScope();
    FixtureUpdateScope(const FixtureUpdateScope&)=delete;
    FixtureUpdateScope& operator=(const FixtureUpdateScope&)=delete;
};
bool observe_fixture(nlohmann::json&,std::string&);
bool validate_fixture_args(const nlohmann::json&,std::string&);
bool run_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
}
