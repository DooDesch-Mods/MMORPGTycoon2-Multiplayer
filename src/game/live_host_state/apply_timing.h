#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <chrono>
#include <cstdint>
namespace mt2mp::game::live_host_state::apply_timing {
enum class Part:size_t {Take,BeforeDay,DayTake,AfterDay,Preflight,FullBefore,Scalar,Motion,FullFinal,EconomyRead,DayRead,MotionOriginal,MemoryVirtualQuery,QueryPlacement,QueryOpenWitness,QueryEngine,QuerySavedFields,QueryWorldProp,QueryOther,BugPreflight,BugAdopt,BugFinish,DeveloperPreflight,DeveloperAdopt,NetworkPreflight,NetworkApply,TutorialPreflight,TutorialApply,PlacementScopedOs,PlacementUnscopedOs,ReaderNestedReset,OriginalNestedReset,QuestStores,NpcStores,PlacementUnscopedDeveloperOs,PlacementUnscopedTutorialOs,PlacementUnscopedEconomyOs,PlacementUnscopedOtherOs,PlacementScopedDeveloperOs,PlacementScopedTutorialOs,PlacementScopedEconomyOs,PlacementScopedOtherOs,ToonNormalize,ToonOriginal,ModeUpdate,ScreenUpdate,ScreenDrawPipeline,QuestToonRead,QuestSubscriberGuard,QuestFreshLinkGuard,QuestScalarQualified,QuestNonqualified,QuestCanonicalOwners,QuestAdmitHistory,QuestRestorePause,QuestStoreSaveEnter,PageScopeMiss,PageScopeBackwardOverlap,WorldPropCurrent,WorldPropTypedLookup,WorldPropRootRegion,WorldPropMainMember,ToonOwnerSession,ToonOwnerNativeOpen,ToonOwnerClock,NativeOpenFrameworkBinding,NativeOpenWorldFields,NativeOpenAccountAttributes,Count};
struct Totals {uint64_t count=0,total_ns=0,max_ns=0;};
// The diagnostic reader sees its game-thread aggregates. Rejected foreign-thread
// calls keep separate counters and cannot race with the bound thread.
inline thread_local std::array<Totals,size_t(Part::Count)> totals{};
inline thread_local size_t take_depth=0;
inline thread_local size_t quest_store_depth=0;
inline void record(Part part,uint64_t ns){auto& t=totals[size_t(part)];++t.count;t.total_ns+=ns;if(ns>t.max_ns)t.max_ns=ns;}
enum class QueryPhase {Other,Developer,Tutorial,Economy};
inline thread_local QueryPhase query_phase=QueryPhase::Other;
inline QueryPhase phase_for(Part part,QueryPhase inherited){
    switch(part){
    case Part::DeveloperPreflight:case Part::DeveloperAdopt:return QueryPhase::Developer;
    case Part::TutorialPreflight:case Part::TutorialApply:return QueryPhase::Tutorial;
    case Part::BeforeDay:case Part::DayTake:case Part::AfterDay:return QueryPhase::Economy;
    case Part::Take:case Part::BugPreflight:case Part::BugAdopt:case Part::BugFinish:
    case Part::NetworkPreflight:case Part::NetworkApply:return QueryPhase::Other;
    default:return inherited;
    }
}
inline Part unscoped_phase_part(QueryPhase phase){
    switch(phase){
    case QueryPhase::Developer:return Part::PlacementUnscopedDeveloperOs;
    case QueryPhase::Tutorial:return Part::PlacementUnscopedTutorialOs;
    case QueryPhase::Economy:return Part::PlacementUnscopedEconomyOs;
    default:return Part::PlacementUnscopedOtherOs;
    }
}
inline Part scoped_phase_part(QueryPhase phase){
    switch(phase){
    case QueryPhase::Developer:return Part::PlacementScopedDeveloperOs;
    case QueryPhase::Tutorial:return Part::PlacementScopedTutorialOs;
    case QueryPhase::Economy:return Part::PlacementScopedEconomyOs;
    default:return Part::PlacementScopedOtherOs;
    }
}
class Scope {
    Part part_;
    QueryPhase previous_phase_=query_phase;
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now();
public:
    explicit Scope(Part part):part_(part){query_phase=phase_for(part_,previous_phase_);if(part_==Part::Take)++take_depth;if(part_==Part::QuestStores)++quest_store_depth;}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    ~Scope(){const auto ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started_).count());if(part_==Part::Take)--take_depth;if(part_==Part::QuestStores)--quest_store_depth;record(part_,ns);query_phase=previous_phase_;}
};
inline nlohmann::json snapshot(){
    static constexpr const char* names[]={"client_take","before_day","day_take","after_day","preflight","full_before","scalar","motion","full_final","economy_native_read","day_native_read","motion_original_calls","memory_virtual_query","query_placement","query_open_witness","query_engine","query_saved_fields","query_world_prop","query_other","bug_preflight","bug_adopt","bug_finish","developer_preflight","developer_adopt","network_preflight","network_apply","tutorial_preflight","tutorial_apply","placement_scoped_os","placement_unscoped_os","reader_nested_reset","original_nested_reset","quest_stores","npc_stores","placement_unscoped_developer_os","placement_unscoped_tutorial_os","placement_unscoped_economy_os","placement_unscoped_other_os","placement_scoped_developer_os","placement_scoped_tutorial_os","placement_scoped_economy_os","placement_scoped_other_os","toon_normalize","toon_original","mode_update","screen_update","screen_draw_pipeline","quest_store_toon_read","quest_store_subscriber_guard","quest_store_fresh_link_guard","quest_store_scalar_qualified","quest_store_nonqualified","quest_store_canonical_owners","quest_store_admit_history","quest_store_restore_pause","quest_store_save_enter","page_scope_miss","page_scope_backward_overlap","world_prop_current","world_prop_typed_lookup","world_prop_root_region","world_prop_main_member","toon_owner_session","toon_owner_native_open","toon_owner_clock","native_open_framework_binding","native_open_world_fields","native_open_account_attributes"};
    static_assert(std::size(names)==size_t(Part::Count));
    nlohmann::json out=nlohmann::json::object();
    for(size_t i=0;i<totals.size();++i){const auto& t=totals[i];out[names[i]]={{"count",t.count},{"total_ns",t.total_ns},{"max_ns",t.max_ns}};}
    return out;
}
}
