#pragma once
#include <string>

namespace mt2mp::game::party_frame {
// An empty Party roster cannot serialize external references. Its complete
// membership is still read twice; nonempty rosters retain the full owner proof.
template<class Raw,class Read,class Pin,class Clear>
bool capture_reference_pins(Read&& read,Pin&& pin,Clear&& clear,std::string& error,
                            bool incoming_parties=false,bool incoming_subscribers=false){
    if(incoming_parties||incoming_subscribers)return pin(error);
    Raw before;
    if(!read(before,error))return false;
    if(!before.members.empty())return pin(error);
    Raw after;
    if(!read(after,error))return false;
    if(before.scope!=after.scope||before.members!=after.members||before.pools!=after.pools){
        error="Party empty capture owner/membership changed during fresh census";
        return false;
    }
    clear();
    return true;
}
}
