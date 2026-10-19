#pragma once
#include "../general_project_codec.h"
namespace mt2mp::action_session {
using GeneralProjectIntent=game::general_project::Intent;
using GeneralProjectProof=game::general_project::Proof;
inline bool general_project_valid(const GeneralProjectIntent& i){return game::general_project::valid(i);}
inline bool general_project_valid(const GeneralProjectProof& p){
    if(!game::general_project::valid(p.selected.source)||!game::general_project::valid(p.progress)||!game::general_project::codec::digest(p.catalogue_sha256)||!game::general_project::codec::digest(p.ownership_sha256))return false;
    for(auto f:p.selected.tech_flags)if(f>1)return false;return true;
}
}
