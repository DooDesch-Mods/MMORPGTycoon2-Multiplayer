#pragma once
#include "typed_action/terrain_action.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::game::ui_terrain_runtime::poly {
using action_session::TerrainPoint;
using action_session::TerrainShape;
struct VectorView {uintptr_t begin=0,end=0,capacity=0;};
static_assert(sizeof(VectorView)==24&&sizeof(TerrainPoint)==16);
inline bool span(const VectorView& v,size_t stride,size_t limit,size_t& count,std::string& e) {
    if(!v.begin) {
        if(v.end||v.capacity){e="terrain native vector has null start and nonempty span";return false;}
        count=0;return true;
    }
    if(v.end<v.begin||v.capacity<v.end||(v.end-v.begin)%stride||(v.capacity-v.begin)%stride||(v.end-v.begin)/stride>limit) {
        e="terrain native vector range/alignment/limit differs";return false;
    }
    count=(v.end-v.begin)/stride;return true;
}
template<class Memory>bool read(Memory& memory,uintptr_t shape,TerrainShape& out,std::string& e) {
    uintptr_t data=0;VectorView paths;
    if(!shape||shape>UINTPTR_MAX-0x18){e="terrain PolyShape address invalid";return false;}
    if(!memory.copy(shape+0x18,&data,sizeof data,e)||!data||!memory.copy(data,&paths,sizeof paths,e))return false;
    size_t count=0;if(!span(paths,24,1000000,count,e))return false;
    TerrainShape result;result.reserve(count);size_t remaining=1000000;
    for(size_t i=0;i<count;++i) {
        VectorView vertices;if(!memory.copy(paths.begin+i*24,&vertices,sizeof vertices,e))return false;
        size_t size=0;if(!span(vertices,16,remaining,size,e))return false;
        auto& path=result.emplace_back(size);
        if(size&&!memory.copy(vertices.begin,path.data(),size*16,e))return false;
        remaining-=size;
    }
    out=std::move(result);return true;
}
// Only the native assignment reads these views. The game's GNU allocator owns
// its deep copy; no MSVC allocation is installed in a native owning vector.
struct BorrowedPaths {
    std::vector<VectorView> paths;
    VectorView view;
    explicit BorrowedPaths(const TerrainShape& shape) {
        paths.reserve(shape.size());
        for(const auto& path:shape) {
            const auto begin=path.empty()?uintptr_t(0):uintptr_t(path.data());
            paths.push_back({begin,begin+path.size()*16,begin+path.size()*16});
        }
        const auto begin=paths.empty()?uintptr_t(0):uintptr_t(paths.data());
        view={begin,begin+paths.size()*24,begin+paths.size()*24};
    }
};
struct alignas(16) Storage {std::array<unsigned char,0x70> bytes{};};
}
