#pragma once
#include <string>
namespace mt2mp::game::node_observer {
bool install(std::string&);
bool suspend_if_prepared(const std::string&,std::string&);
}
