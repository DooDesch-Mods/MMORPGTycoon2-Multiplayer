#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace mt2mp::hooks::framework {
// A pin retains only digest results for exact freshly supplied code bytes.
// Two observations cover the physical and reconstructed original hook bodies.
class CodeBodyDigest {
    struct Observation {uintptr_t entry=0;std::string expected,digest;std::vector<unsigned char> bytes;};
    std::array<Observation,2> observations_{};
    size_t next_=0;
public:
    template<class Hash>
    bool read(uintptr_t entry,const void* bytes,size_t size,const char* expected,
              std::string& digest,std::string& error,Hash&& hash){
        for(const auto& saved:observations_)if(saved.entry==entry&&saved.expected==expected&&saved.bytes.size()==size&&
            !std::memcmp(saved.bytes.data(),bytes,size)){digest=saved.digest;return true;}
        std::string calculated;if(!hash(calculated,error))return false;
        const auto* begin=static_cast<const unsigned char*>(bytes);
        Observation saved{entry,expected,calculated,std::vector<unsigned char>(begin,begin+size)};
        observations_[next_]=std::move(saved);next_=(next_+1)%observations_.size();digest=std::move(calculated);return true;
    }
};
}
