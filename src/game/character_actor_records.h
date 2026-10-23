#pragma once
#include "character_membership_reader.h"
#include "character_catalogue_proof.h"
#include "character_catalogue_batch.h"
#include "character_catalogue_record.h"
#include "character_catalogue_timing.h"
#include "subscriber_lifecycle/record_graph.h"
#include "subscriber_lifecycle/capture.h"
#include "duplicate_records.h"
#include "typed_action/action_session.h"
#include <functional>

namespace mt2mp::game::npc_probe::character_actor_records {
using Family=character_membership_reader::Family;
inline constexpr std::array<std::array<uintptr_t,2>,4> methods{{
    {0x10eda70,0x10edad0},{0xe4ad10,0xe4ad70},{0x10ecae0,0x10ecb40},{0x10f9c20,0x10f9c80}}};
using External=std::function<bool(uintptr_t,std::string&,std::string&)>;
struct Result {
    subscriber_lifecycle::Graph original;
    duplicate_records::Witness portable;
    subscriber_lifecycle::CaptureReceipt cleanup;
};
bool read_current(const action_session::Context&,const action_session::Binding&,
                  character_definition_identity::Census&,character_membership_reader::Users&,std::string&);
struct CatalogueProof {std::string sha256;bool operator==(const CatalogueProof&)const=default;};
bool capture_catalogue(const action_session::Context&,const action_session::Binding&,CatalogueProof&,std::string&);
bool read_definition(const action_session::Context&,const action_session::Binding&,
                     int32_t kind,int32_t global_id,std::string& original_record_sha,std::string&);
// The complete original tree and its separate canonical reference manifest are
// retained locally. The caller supplies current external owners; an unknown
// reference fails rather than substituting a native pointer or display name.
bool read(const action_session::Binding&,const std::string& snapshot,Family,
          uintptr_t actor,const External&,Result&,std::string&);
}
