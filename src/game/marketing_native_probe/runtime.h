#pragma once
#include "contract.h"
#include <filesystem>
namespace mt2mp::game::marketing_native_probe {
// One owned Client diagnostic attempt per loaded world. No automatic retry.
bool probe(const std::filesystem::path&,const Json& arguments,Json& receipt,std::string&);
}
