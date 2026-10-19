#pragma once
#include <cstddef>
#include <functional>
#include <string>

namespace mt2mp::subscriber_lifecycle::first_append {
struct PartyToonOrderReceipt {
    size_t loads_attempted=0,loads_returned=0,resolves_attempted=0,resolves_returned=0;
    bool parties_attempted=false,parties_returned=false;
};
// Each Subscriber keeps its original saved-link namespace. Party completion
// consumes constructor results before any Subscriber PostResolve can read it.
inline bool party_toon_order(size_t count,const std::function<bool(size_t,std::string&)>& load,
    const std::function<bool(std::string&)>& parties,
    const std::function<bool(size_t,std::string&)>& resolve,PartyToonOrderReceipt& receipt,std::string& error){
    receipt={};
    if(!count||!load||!parties||!resolve){error="Party/Toon original call order binding incomplete";return false;}
    for(size_t i=0;i<count;++i){++receipt.loads_attempted;if(!load(i,error))return false;++receipt.loads_returned;}
    receipt.parties_attempted=true;
    if(!parties(error))return false;
    receipt.parties_returned=true;
    for(size_t i=0;i<count;++i){++receipt.resolves_attempted;if(!resolve(i,error))return false;++receipt.resolves_returned;}
    return true;
}
}
