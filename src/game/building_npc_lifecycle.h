#pragma once
#include "npc_saved_identity.h"
#include "npc_quest_records.h"
#include <functional>

namespace mt2mp::game::building_npc_lifecycle {
// These receipts are local native witnesses. Pointers and native UIDs must not
// be serialized into an action packet. World owns the original native graph.
struct Lease {
    std::string snapshot,request_uuid,duplicate_uuid;
    uint64_t epoch=0,generation=0;
};
enum class MakeForm {Unknown,Functional,Modular,Art};
struct Parent {
    std::string key;uintptr_t object=0;uint64_t uid=0;
    int32_t saved_region=-1;
    MakeForm make_form=MakeForm::Unknown;
    uint64_t stored_child_uid=0;
};
struct Receipt {
    Lease lease;Parent parent;npc_saved_identity::Current child;
    bool present=false;
    std::string original_record_sha;
    uintptr_t original_shard=0,original_region=0;
    uint64_t original_character_uid=0,inverse_child_uid=0;
    std::array<unsigned char,0x6c> original_transform{};
    std::string original_display_name;
    npc_quest_records::State original_quests;
    std::vector<uint64_t> original_destination_uids;
};
struct MakeNPCInverse {
    uintptr_t node=0;uint64_t produced_uid=0,required_character_uid=0;
};
// This resolver receives external owners while an original private SaveContext
// is alive. It must use the caller's current canonical census, and must not
// reenter the saved-NPC reader that owns this receipt's scope.
using OwnerResolver=std::function<bool(uintptr_t,std::string&,std::string&)>;

// Called after the original Duplicate finishes and before its record/Quest
// readback. Existing serialized keys are retained; new children use their
// canonical Building owner. An empty original a30 produces an explicit no-child
// receipt rather than inventing an NPC binding.
bool capture_birth(const Lease&,const Parent&,const OwnerResolver&,Receipt&,std::string&);
// Capture on the pending Undo lease before the original whole graph executes.
bool capture_before(const Lease&,const Parent&,const OwnerResolver&,Receipt&,std::string&);
// Confirm only after original queued deletion. This function never destroys an
// actor, and a still-present UID or complete-census member remains a failure.
bool confirm_retired(const Lease&,const Receipt&,std::string&);
// Read the original node emitted by NPC::Destroy and validate its exact native
// produced/required singleton UID sets. World proves its unique graph membership.
bool validate_make_npc(const Lease&,uintptr_t node,Receipt&,MakeNPCInverse&,std::string&);
// Called after original Redo Make. The complete original MakeNPC inverse owns
// its saved UID, including Modular Building graphs. The new pointer must come
// from the original reciprocal parent and complete current census.
bool confirm_restored(const Lease&,const Receipt&,const Parent&,const OwnerResolver&,Receipt&,std::string&);
// Supports an explicit remap of previous local Quest owners after original
// restoration. The caller supplies the current full UID from the confirmed
// restored receipt. A stale process pointer is never treated as a live owner.
bool resolve(const std::string& key,uint64_t uid,npc_saved_identity::Current&,std::string&);
// Copy validated current child aliases before entering a private Record capture.
// Its owner callback can then use this local census without nested reader locks.
bool current_aliases(std::vector<npc_saved_identity::Current>&,std::string&);
}
