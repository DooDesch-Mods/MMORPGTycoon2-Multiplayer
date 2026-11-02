#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace mt2mp::game::management_service_graph {
// These guards are for local pointer descriptors. Addresses and capacities do
// not enter the canonical projection.
inline bool array_span(uintptr_t data,int32_t count,int32_t capacity,size_t stride,size_t limit){
    if(count<0||capacity<count||size_t(count)>limit||size_t(capacity)>limit||!stride)return false;
    if(!data)return capacity==0;
    return size_t(capacity)<=std::numeric_limits<uintptr_t>::max()/stride&&
        data<=std::numeric_limits<uintptr_t>::max()-size_t(capacity)*stride;
}
inline bool vector_span(uintptr_t begin,uintptr_t end,uintptr_t capacity,size_t stride,size_t limit,size_t& count){
    count=0;if(!stride||end<begin||capacity<end)return false;
    if(!begin)return end==0&&capacity==0;
    if((end-begin)%stride||(capacity-begin)%stride||(capacity-begin)/stride>limit)return false;
    count=size_t((end-begin)/stride);return true;
}
inline bool bit_span(uintptr_t begin,uint32_t first_bit,uintptr_t end,uint32_t end_bit,uintptr_t capacity,size_t expected){
    if(first_bit!=0||end_bit>=32||end<begin||capacity<end)return false;
    if(!begin)return end==0&&capacity==0&&end_bit==0&&expected==0;
    if((end-begin)%4||(capacity-begin)%4)return false;
    const auto bytes=end-begin;if(bytes>(std::numeric_limits<size_t>::max()-end_bit)/8)return false;
    const auto length=size_t(bytes)*8+end_bit;
    return length==expected&&(end_bit==0||capacity-end>=4);
}
}
