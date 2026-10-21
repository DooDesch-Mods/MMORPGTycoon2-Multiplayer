#pragma once
#include "toon_thought_state.h"
#include <algorithm>
namespace mt2mp::game::toon_thought {
struct ZoneList {Array array;std::vector<uintptr_t> members;bool operator==(const ZoneList&)const=default;};
template<class Read>bool zone_list(Read& at,uintptr_t image,uintptr_t zone,ZoneList& out,std::string& e){
    out={};uintptr_t ap=0;if(!at(zone,0,ap,e)||ap!=image+0x1745ce0||!at(zone,0x580,ap,e)||ap!=image+0x1717220||!array(at,zone,0x5a0,out.array,e))return fail(e,e.empty()?"Thought current Zone/link-array class differs":e);
    std::set<uintptr_t> distinct;for(int32_t i=0;i<out.array.count;++i){uintptr_t value=0;if(!at(out.array.data,size_t(i)*8,value,e))return false;if(!value||!distinct.insert(value).second)return fail(e,"Thought Zone link list contains a null or repeated opaque identity");out.members.push_back(value);}return true;
}
// The original remover compares this identity against slots, then shifts them.
// It never reads memory through the Thought identity.
template<class Read,class Current,class Remove>bool remove_zone_link(Read& at,Current& current,Remove& remove,uintptr_t image,uintptr_t zone,uintptr_t thought,bool& attempted,std::string& e){
    ZoneList before;if(!current(e)||!zone_list(at,image,zone,before,e))return false;
    const auto found=std::find(before.members.begin(),before.members.end(),thought);if(found==before.members.end())return fail(e,"Thought Zone removal requires exact current membership");
    ZoneList fresh;if(!current(e)||!zone_list(at,image,zone,fresh,e)||fresh!=before)return fail(e,e.empty()?"Thought Zone list changed before original removal":e);
    attempted=true;if(!remove(zone,thought,e))return false;
    ZoneList after;if(!current(e)||!zone_list(at,image,zone,after,e))return false;
    auto expected=before.members;expected.erase(expected.begin()+(found-before.members.begin()));
    if(after.array.owner!=before.array.owner||after.array.data!=before.array.data||after.array.capacity!=before.array.capacity||after.members!=expected||after.array.count!=before.array.count-1)return fail(e,"Thought original Zone removal complete ordered readback differs");e.clear();return true;
}
}
