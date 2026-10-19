#pragma once
#include "boss_identity.h"
#include "character_actor_records.h"
#include "native_open_witness.h"
namespace mt2mp::game::boss_current {
struct Pool {boss_identity::Scope scope;character_boss_retirement_read::Pool native;};
struct State {
    building_mixed_identity::opened_file::CurrentWorld world;
    std::vector<Pool> pools;
    std::vector<std::pair<std::string,boss_identity::Entry>> live;
};
struct Retirement {
    action_session::Context context;action_session::Binding binding;
    State before;std::map<uintptr_t,std::vector<boss_identity::Member>> selected;
    bool confirmed=false;
};
struct Birth {
    action_session::Context context;action_session::Binding binding;
    State before;boss_identity::Scope scope;std::string key;
    boss_identity::Entry created;bool attempted=false,confirmed=false,unknown=false;
    std::string cause;
};
// Prepare before the original Cursor transaction. Confirm immediately after its
// actual AddBoss return, before any complete post-action Record capture.
bool prepare_birth(const action_session::Context&,const action_session::Binding&,
                   uintptr_t manager,const std::string& action_uuid,uint32_t ordinal,
                   Birth&,std::string&);
bool confirm_birth(Birth&,uintptr_t original_returned_boss,std::string&);
bool read(const action_session::Context&,const action_session::Binding&,State&,std::string&);
// Return the fresh Character census already required by this Boss admission.
bool read_with_characters(const action_session::Context&,const action_session::Binding&,State&,
                          character_definition_identity::Census&,character_membership_reader::Users&,std::string&);
bool prepare_retirement(const action_session::Context&,const action_session::Binding&,
                        const std::vector<boss_identity::Member>&,Retirement&,std::string&);
// Call after the one original Destroy and before post-action Record capture.
bool confirm_retirement(Retirement&,std::string&);
}
