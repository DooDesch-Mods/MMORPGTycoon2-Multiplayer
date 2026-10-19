#pragma once
#include "vendor/nlohmann/json.hpp"
#include "cohort.h"
#include "native_calls.h"
#include "typed_action/action_session.h"
#include <filesystem>
#include <functional>
#include <memory>
#include "save_scope.h"
#include "reference_map.h"
namespace mt2mp::game::subscriber_lifecycle_append {
bool initialize(uintptr_t image,std::string&);
void* lookup_entrypoint();
void set_lookup_hook_ready(bool);
void* lookup(void* context,int32_t id,mt2mp::subscriber_lifecycle::Lookup original);
// Root stages the authenticated Host artifact in this Client's private record directory.
bool probe(const std::filesystem::path& runtime_root,const std::string& artifact_basename,
           const std::string& artifact_sha,const std::string& actual_client_origin,
           nlohmann::json& receipt,std::string&);
struct FrameBinding {
    action_session::Context context;
    action_session::Binding binding;
    uint64_t common_epoch=0;
    std::string local_sha;
};
// Local-only pointers. fresh must revalidate the Instance owner and the Quest's
// canonical NPC key/sequence, including current owner and Manager membership.
// quest==0 explicitly requests Empty. Then portable_key identifies the owning NPC,
// and fresh receives {0,key,0} to validate Toon/Progress/Instance ownership only.
// An expired old Proxy is released by original Resolve, not treated as Empty.
struct QuestLinkRequest {
    uintptr_t instance=0,quest=0;
    std::string portable_key;
    mt2mp::subscriber_lifecycle::FreshLink fresh=nullptr;
    void* user=nullptr;
};
struct QuestLinkReceipt {
    mt2mp::subscriber_lifecycle::CallReceipt construct,resolve,abort,destroy;
    mt2mp::subscriber_lifecycle::LookupReceipt lookup;
    int32_t local_id_before=0,local_id_after=0;
    bool before_confirmed=false,id_store_attempted=false,id_restored=false;
    bool after_confirmed=false,context_closed=false,private_context_retained=false;
    bool unknown=false,confirmed=false;
};
bool bind_quest_instance(const QuestLinkRequest&,const std::string& actual_client_origin,
                         const FrameBinding&,QuestLinkReceipt&,std::string&);

// Local pointers only. fresh validates Toon ownership and the canonical NPC key.
// It must not dereference a retiring Progress row, including after its D0.
struct ProgressRequest {
    uintptr_t toon=0,npc=0;
    std::string npc_key;
    mt2mp::subscriber_lifecycle::FreshLink fresh=nullptr;
    void* user=nullptr;
};
struct ProgressReceipt {
    uintptr_t progress=0,instance=0;
    int32_t count_before=0,count_after=0;
    mt2mp::subscriber_lifecycle::CallReceipt allocate,construct,assign,append,shift,destroy;
    bool existing=false,completed_initialized=false,published=false,removed=false;
    bool after_confirmed=false,unknown=false,retained=false,confirmed=false;
};
bool ensure_progress(const ProgressRequest&,const std::string& actual_client_origin,
                     const FrameBinding&,ProgressReceipt&,std::string&);
bool retire_progress(const ProgressRequest&,uintptr_t current_progress,
                     const std::string& actual_client_origin,const FrameBinding&,
                     ProgressReceipt&,std::string&);

// A held helper reports its body result. Final confirmation belongs to the batch
// after original leave and the closed Client owner proof.
struct QuestEditBatch;
bool quest_edits_held(const QuestEditBatch&)noexcept;
bool verify_quest_edits_held(QuestEditBatch&,std::string&);
bool with_quest_edit_row(QuestEditBatch&,
    const std::function<bool(QuestEditBatch&,std::string&)>&,
    const std::function<bool(std::string&)>&,std::string&);
bool with_quest_edits(const std::string& actual_client_origin,const FrameBinding&,
    const std::function<bool(QuestEditBatch&,std::string&)>& body,
    const std::function<bool(std::string&)>& exit_guard,
    mt2mp::subscriber_lifecycle::SaveScopeReceipt&,std::string&);
bool bind_quest_instance(QuestEditBatch&,const QuestLinkRequest&,const std::string&,
                         const FrameBinding&,QuestLinkReceipt&,std::string&);
bool ensure_progress(QuestEditBatch&,const ProgressRequest&,const std::string&,
                     const FrameBinding&,ProgressReceipt&,std::string&);
bool retire_progress(QuestEditBatch&,const ProgressRequest&,uintptr_t,const std::string&,
                     const FrameBinding&,ProgressReceipt&,std::string&);

bool apply_frame(const nlohmann::json&,const std::string& actual_client_origin,const FrameBinding&,nlohmann::json& receipt,std::string&);
bool apply_frame(const nlohmann::json&,const std::string& actual_client_origin,const FrameBinding&,
                 mt2mp::subscriber_lifecycle::first_append::PartyReferences&,nlohmann::json& receipt,std::string&);
struct PartyToonBatch;
struct PartyToonInputs;
struct PendingToon {
    int32_t subscriber_id=-1;
    uintptr_t subscriber=0,toon=0;
    uint64_t uid=0;
    bool operator==(const PendingToon&)const=default;
};
// Local constructor results only. They are valid while this original drained
// SaveScope is held, before Subscriber Resolve/PostResolve has run.
bool verify_party_toon_edits_held(PartyToonBatch&,std::string&);
bool pending_toon(PartyToonBatch&,int32_t subscriber_id,PendingToon&,std::string&);
// Pin existing Building/NPC owners while their original readers require SaveIdle.
bool prepare_party_toons(const std::vector<nlohmann::json>&,const std::string& actual_client_origin,
    const FrameBinding&,std::shared_ptr<PartyToonInputs>&,std::string&);
// Borrows the existing Party NativePort::batch SaveScope. Its owner performs the
// one original leave and final closed confirmation after this body succeeds.
bool with_party_toon_edits_held(const std::string& actual_client_origin,const FrameBinding&,
    const std::function<bool(PartyToonBatch&,std::string&)>& body,
    const std::function<bool(std::string&)>& exit_guard,
    mt2mp::subscriber_lifecycle::SaveScopeReceipt&,std::string&);
// Loads every Subscriber before calling finish_parties. That callback performs
// original Party Load/Resolve with the real pending Toons and reciprocal links,
// deferring InitMapData and aliases. rearm_party calls original PropertyLoad only:
// zero before Party load, then the exact own saved ID before Subscriber D1.
// finalize_parties runs after every Subscriber D1 and login, so original Party
// InitMapData and final readback see resolved Region/Subscriber pointers.
// A true result is held-body readback; the outer SaveScope owns confirmation.
bool append_party_toons(PartyToonBatch&,const std::shared_ptr<PartyToonInputs>&,
    mt2mp::subscriber_lifecycle::first_append::PartyReferences&,
    const std::function<bool(const std::vector<PendingToon>&,std::string&)>& finish_parties,
    const std::function<bool(const PendingToon&,int32_t,void*,std::string&)>& rearm_party,
    const std::function<bool(std::string&)>& finalize_parties,
    nlohmann::json& receipt,std::string&);
bool apply_graph(const nlohmann::json&,const std::string& actual_client_origin,nlohmann::json& receipt,std::string&);
}
