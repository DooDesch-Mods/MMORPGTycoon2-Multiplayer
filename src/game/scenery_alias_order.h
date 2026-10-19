#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::scenery_alias_order {
struct Receipt {std::vector<std::string> retired,restored;};
// The native census and graph are validated by the caller. The shared Record
// reader validates every live alias, so exact retirement/restoration must
// precede that reader even though shared Table publication remains deferred.
template<class Retire,class Restore,class Capture>
bool capture(const std::map<uint64_t,std::string>& before,const std::set<uint64_t>& current,
             const std::map<uint64_t,std::string>& restorable,Retire&& retire,Restore&& restore,Capture&& records,
             Receipt& out,std::string& e){
    out={};std::vector<uint64_t> removed;std::set<std::string> expected;
    for(const auto& [uid,key]:before)if(!current.contains(uid)){removed.push_back(uid);if(!expected.insert(key).second){e="Scenery alias retirement canonical key repeats";return false;}}
    if(!removed.empty()&&!retire(removed,out.retired,e))return false;
    if(out.retired.size()!=expected.size()||std::set<std::string>(out.retired.begin(),out.retired.end())!=expected){e="Scenery exact retired aliases differ before Record capture";return false;}
    for(const auto& [uid,key]:restorable)if(!before.contains(uid)&&current.contains(uid)){
        if(!restore(uid,key,e))return false;out.restored.push_back(key);
    }
    return records(e);
}
}
