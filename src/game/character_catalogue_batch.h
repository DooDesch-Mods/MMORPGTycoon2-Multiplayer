#pragma once
#include "character_catalogue_proof.h"

namespace mt2mp::game::character_catalogue_batch {
namespace definitions=character_definition_identity;
enum class Mode { LinkFree, Eager };
enum class Result { Complete, NeedsExternalOwners, Failed };
inline bool same(const definitions::Array& a,const definitions::Array& b){
    return a.object==b.object&&a.data==b.data&&a.count==b.count&&a.capacity==b.capacity&&a.members==b.members;
}
inline bool same(const definitions::Census& a,const definitions::Census& b){
    if(a.state!=b.state||a.manager!=b.manager||a.definitions.size()!=b.definitions.size()||a.collections.size()!=b.collections.size())return false;
    for(size_t i=0;i<a.arrays.size();++i)if(!same(a.arrays[i],b.arrays[i]))return false;
    for(size_t i=0;i<a.definitions.size();++i){const auto& x=a.definitions[i];const auto& y=b.definitions[i];
        if(x.key!=y.key||x.kind!=y.kind||x.id!=y.id||x.variant_slot!=y.variant_slot||x.object!=y.object||x.parent!=y.parent||
           x.owning_collection!=y.owning_collection||x.variants!=y.variants||x.record!=y.record||x.uid!=y.uid||x.parent_uid!=y.parent_uid||
           x.deleted!=y.deleted||x.edited!=y.edited||x.released!=y.released)return false;
    }
    for(size_t i=0;i<a.collections.size();++i){const auto& x=a.collections[i];const auto& y=b.collections[i];
        if(x.object!=y.object||x.owner!=y.owner||x.ids!=y.ids)return false;
        for(size_t j=0;j<x.arrays.size();++j)if(!same(x.arrays[j],y.arrays[j]))return false;
    }
    return true;
}

// A preparation validates the current census, users and world, plus all external
// names in eager mode. Each serializer retains the caller's lifetime checks.
// No record bytes are reused between the two complete passes.
template<class Sample,class Prepare,class Record,class Stable>
Result capture_attempt(Prepare&& prepare,Record&& record,Stable&& stable,nlohmann::json& out,std::string& error){
    out=nullptr;
    const auto failed=[&](const char* cause){definitions::fail(error,cause);return Result::Failed;};
    Sample current;if(!prepare(current,error))return Result::Failed;
    nlohmann::json first;
    for(size_t pass=0;pass<2;++pass){
        std::map<std::string,std::string> records;
        for(const auto& definition:current.definitions.definitions)if(!definition.deleted){std::string value;
            const auto result=record(current,definition,value,error);if(result!=Result::Complete)return result;
            if(!records.emplace(definition.key,std::move(value)).second)
                return failed("Character catalogue repeats an original global key");
        }
        nlohmann::json rows;if(!character_catalogue_proof::rows(current.definitions,records,rows,error))return Result::Failed;
        Sample after;if(!prepare(after,error))return Result::Failed;
        if(!stable(current,after))return failed("Character catalogue original records/manager/users/reference owners changed during complete capture");
        if(!pass)first=std::move(rows);
        else if(first!=rows)return failed("Character catalogue original records changed during complete double capture");
        current=std::move(after);
    }
    out=std::move(first);error.clear();return Result::Complete;
}
template<class Sample,class Prepare,class Record,class Stable>
bool capture(Prepare&& prepare,Record&& record,Stable&& stable,nlohmann::json& out,std::string& error){
    auto tagged=[&](const auto& current,const auto& definition,auto& value,auto& cause){
        return record(current,definition,value,cause)?Result::Complete:Result::Failed;
    };
    return capture_attempt<Sample>(prepare,tagged,stable,out,error)==Result::Complete;
}

// An external manifest entry requires a fresh eager capture from the beginning.
// A map collected after its serializer cannot establish that record's pre-state.
template<class Sample,class Prepare,class Record,class Stable>
bool capture_manifest(Prepare&& prepare,Record&& record,Stable&& stable,nlohmann::json& out,std::string& error){
    auto attempt=[&](Mode mode){
        auto preparing=[&](auto& value,auto& cause){return prepare(mode,value,cause);};
        auto recording=[&](const auto& value,const auto& definition,auto& sha,auto& cause){return record(mode,value,definition,sha,cause);};
        return capture_attempt<Sample>(preparing,recording,stable,out,error);
    };
    const auto first=attempt(Mode::LinkFree);
    if(first!=Result::NeedsExternalOwners)return first==Result::Complete;
    const auto eager=attempt(Mode::Eager);
    if(eager==Result::NeedsExternalOwners)return definitions::fail(error,"Character catalogue eager capture requested external owners again");
    return eager==Result::Complete;
}
}
