#pragma once
#include "saved_scenery_reader.h"
#include <map>
#include <optional>
#include <set>

namespace mt2mp::build_actions::private_native::host {
// One synchronous readonly traversal. The caller must finalize before publishing.
class SavedSceneryBatch {
    uintptr_t image_=0,map_=0,root_=0;
    uint64_t boundary_=0;
    bool initialized_=false,failed_=false;
    std::string cause_;
    std::optional<mt2mp::game::region_saved_identity::Index> index_;
    std::map<uintptr_t,place::SceneryWitness> regions_;
    std::map<uintptr_t,std::pair<uint64_t,uintptr_t>> members_;
    std::set<uint64_t> uids_;
    bool stop(std::string& error,const char* fallback){
        failed_=true;if(cause_.empty())cause_=error.empty()?fallback:error;error=cause_;return false;
    }
public:
    bool healthy(std::string& error)const{if(failed_){error=cause_;return false;}return true;}
    bool reject(std::string& error){return stop(error,"saved Scenery readonly batch failed");}
    bool admit(uintptr_t image,uintptr_t map,uintptr_t root,uint64_t boundary,std::string& error){
        if(failed_){error=cause_;return false;}
        if(!image||!map||map>UINTPTR_MAX-0x1c0||root!=map+0x1c0||boundary==UINT64_MAX){error="saved Scenery batch binding or native boundary exhausted";return stop(error,"");}
        if(!initialized_){image_=image;map_=map;root_=root;boundary_=boundary;initialized_=true;return true;}
        if(image!=image_||map!=map_||root!=root_||boundary!=boundary_){error="saved Scenery batch native boundary or owner changed";return stop(error,"");}
        return true;
    }
    bool active()const{return initialized_;}
    template<class Capture,class Read>
    bool index(uintptr_t image,uintptr_t map,uintptr_t root,mt2mp::game::region_saved_identity::Index& out,
        Read& read,std::string& error,Capture& capture){
        out={};if(failed_){error=cause_;return false;}
        if(!initialized_||image!=image_||map!=map_||root!=root_){error="saved Scenery batch index owner differs";return stop(error,"");}
        if(!index_){mt2mp::game::region_saved_identity::Index fresh;if(!capture(image,map,root,fresh,read,error))return stop(error,"saved Scenery batch index capture failed");index_=std::move(fresh);}
        out=*index_;return true;
    }
    bool member(Memory& memory,place::SceneryHost& owner,uintptr_t image,uintptr_t region,uintptr_t root,uintptr_t manager,uintptr_t object,uint64_t uid,std::string& error){
        if(failed_){error=cause_;return false;}
        place::SceneryContext context;if(!owner.current(context,error))return stop(error,"saved Scenery batch current owner failed");
        if(!initialized_||image!=image_||context.shard!=root_||!index_||
           std::none_of(index_->saved.begin(),index_->saved.end(),[&](const auto& row){return row.second==context.region;})){
            error="saved Scenery batch Region absent complete saved index";return stop(error,"");
        }
        if(context.region!=region||context.shard!=root||context.manager!=manager){error="saved_scenery_current_owner_context_differs";return stop(error,"");}
        const auto found=regions_.find(context.region);
        if(found!=regions_.end()){
            if(found->second.context!=context){error="saved Scenery batch cached Region owner changed";return stop(error,"");}
            const auto match=members_.find(object);
            if(match==members_.end()||match->second.first!=uid||match->second.second!=region){error="saved_scenery_absent_complete_category_census";return stop(error,"");}
            return true;
        }
        place::SceneryWitness fresh;if(!place::read_scenery(memory,owner,image,fresh,error))return stop(error,"saved Scenery batch complete census failed");
        if(fresh.context!=context){error="saved Scenery batch first Region owner changed";return stop(error,"");}
        for(const auto& member:fresh.members)if(!members_.emplace(member.object,std::pair{member.uid,context.region}).second||!uids_.insert(member.uid).second){
            error="saved Scenery batch full UID or receiver spans multiple Regions";return stop(error,"");}
        regions_.emplace(context.region,std::move(fresh));
        const auto match=members_.find(object);
        if(match==members_.end()||match->second.first!=uid||match->second.second!=region){error="saved_scenery_absent_complete_category_census";return stop(error,"");}
        return true;
    }
    template<class Boundary,class Capture,class Read,class Region>
    bool finish(Boundary& boundary,Capture& capture,Read& read,Region& region,std::string& error){
        if(failed_){error=cause_;return false;}
        if(!initialized_)return true;
        if(!admit(image_,map_,root_,boundary(),error))return false;
        mt2mp::game::region_saved_identity::Index before,after;
        if(!capture(image_,map_,root_,before,read,error))return stop(error,"saved Scenery batch final index failed");
        if(!index_||before!=*index_){error="saved Scenery batch complete Region index changed before publication";return stop(error,"");}
        for(const auto& [pointer,expected]:regions_){(void)pointer;place::SceneryWitness actual;
            if(!region(expected.context,actual,error))return stop(error,"saved Scenery batch final census failed");
            if(actual!=expected){error="saved Scenery batch complete Region members or values changed before publication";return stop(error,"");}}
        if(!capture(image_,map_,root_,after,read,error))return stop(error,"saved Scenery batch final index failed");
        if(after!=*index_){error="saved Scenery batch complete Region index changed during final census";return stop(error,"");}
        return admit(image_,map_,root_,boundary(),error);
    }
};
}
