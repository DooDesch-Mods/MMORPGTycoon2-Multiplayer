#pragma once
#include <cstddef>
#include <string>
namespace mt2mp::game::duplicate_records {
struct Witness {
    std::string sha;
    size_t records=0,tokens=0,links=0;
    bool operator==(const Witness&)const=default;
};
}
