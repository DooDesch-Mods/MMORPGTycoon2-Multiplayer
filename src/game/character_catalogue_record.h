#pragma once
#include "character_catalogue_batch.h"
#include "duplicate_records.h"

namespace mt2mp::game::character_catalogue_record {
using Mode=character_catalogue_batch::Mode;
using Result=character_catalogue_batch::Result;
inline Result summarize(const subscriber_lifecycle::Graph& graph,uintptr_t root,
    const std::map<uintptr_t,std::string>& owners,Mode mode,std::string& hash,std::string& error){
    hash.clear();bool needs_owners=false;
    const auto external=[&](uintptr_t pointer,std::string& key,std::string& cause){
        if(mode==Mode::LinkFree){needs_owners=true;return false;}
        const auto found=owners.find(pointer);
        if(found==owners.end()){cause="Character complete original definition Record contains an unresolved external reference";return false;}
        key=found->second;return true;
    };
    duplicate_records::Witness result;
    // The canonicalizer alone decides which manifest entries need external names.
    // Root and inline entries keep its existing validation and representation.
    if(!duplicate_records::summarize(graph,root,external,result,error)){
        if(!needs_owners)return Result::Failed;
        // The callback stops canonicalization at the first external entry. Its
        // remaining IDs must still form a complete manifest before recapture.
        // The canonicalizer already checked all duplicate/inline/object rules.
        std::set<int32_t> ids;for(const auto& link:graph.private_links)ids.insert(link.saved_link_id);
        int32_t expected=1;for(const auto id:ids)if(id!=expected++){
            error="Duplicate original SaveContext IDs are not complete from one";return Result::Failed;
        }
        return Result::NeedsExternalOwners;
    }
    hash=std::move(result.sha);return Result::Complete;
}
}
