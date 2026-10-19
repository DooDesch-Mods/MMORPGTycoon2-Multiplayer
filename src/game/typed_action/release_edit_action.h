#pragma once
#include "../release_edit_codec.h"
namespace mt2mp::action_session {
using ReleaseEditIntent=game::release_edit::Intent;
using ReleaseEditProof=game::release_edit::Proof;
inline bool release_edit_valid(const ReleaseEditIntent& v){return game::release_edit::valid(v);}
inline bool release_edit_valid(const ReleaseEditProof& v){return game::release_edit::valid(v);}
}
