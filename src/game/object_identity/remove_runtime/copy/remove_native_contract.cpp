#include "remove_native_contract.h"
#include <limits>

namespace mt2mp::build_actions::private_native {
bool resolve_remove(Memory& m,uintptr_t image,std::string_view hash,RemoveEntrypoints& out,std::string& e){
    out={};e.clear();
    if(hash!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"){e="remove_exe_hash_mismatch";return false;}
    if(!image||image>std::numeric_limits<uintptr_t>::max()-0x5449c0){e="remove_code_image_range";return false;}
    try {
        for(const auto& g:remove_code_guards){
            std::array<uint8_t,32> actual{};
            const size_t count=g.hex.size()/2;
            if(g.hex.size()%2||count>actual.size()){e="remove_internal_guard_size";return false;}
            if(!m.copy(image+g.rva,actual.data(),count,e)){if(e.empty())e="remove_code_read_failed";return false;}
            for(size_t i=0;i<count;++i){
                auto nibble=[](char c){return c<='9'?c-'0':c-'a'+10;};
                const auto expected=static_cast<uint8_t>(nibble(g.hex[i*2])*16+nibble(g.hex[i*2+1]));
                if(actual[i]!=expected){e="remove_code_guard_mismatch_rva_"+std::to_string(g.rva);return false;}
            }
        }
        out={reinterpret_cast<DestroyProp>(image+0x2d5520),reinterpret_cast<SetHighlightedProps>(image+0x2d3960),
             reinterpret_cast<ClearSelectionFn>(image+0x2d3f90),reinterpret_cast<UndoOpenFn>(image+0x539990),
             reinterpret_cast<GraphLockFn>(image+0x5449a0),reinterpret_cast<PropDestructorFn>(image+0x4d05c0)};
        return true;
    }catch(const std::exception& x){e=std::string("remove_resolver_exception: ")+x.what();return false;}
    catch(...){e="remove_resolver_unknown_exception";return false;}
}
}
