#pragma once
#include "snapshot_save_work_witness.h"
#include "region_rng_capture.h"
#include "mt2mp/game/native_engine.h"
#include <windows.h>
#include <bcrypt.h>
#include "npc_probe.h"
#include "snapshot_construction.h"

namespace mt2mp::game::snapshot_save_observation {
using Json=nlohmann::json;
inline void require(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
struct Memory:npc_census::Memory {
    bool copy(uintptr_t p,void* out,size_t bytes,std::string& error)override{
        SIZE_T copied=0;
        if(!p||p>UINTPTR_MAX-bytes||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),out,bytes,&copied)||copied!=bytes){
            error="Held save observation native read failed address="+std::to_string(p)+" bytes="+std::to_string(bytes);return false;
        }
        return true;
    }
    template<class T>T at(uintptr_t p,size_t offset=0){T value{};std::string error;
        require(p<=UINTPTR_MAX-offset,"Held save observation address overflow");
        require(copy(p+offset,&value,sizeof(value),error),error);return value;
    }
};
inline Json ledger(const native::SimulationState& s){
    return {{"cash",s.cash},{"flat_day",s.clock.flat_day},{"record_present",s.record_present},
        {"record_day",s.record_day},{"record_cash",s.record_cash},{"record_income",s.record_income},
        {"record_expense",s.record_expense},{"line_counts",s.line_counts},{"line_income",s.line_income}};
}
inline Json read(bool shared=false,bool initial=false){
    require(!initial||shared,"Initial held witness requires shared canonical identities");
    uintptr_t image=0;uint32_t thread=0;std::string sha,error;
    require(native::read_resolver_receipt(image,thread,sha,error),error);
    require(sha=="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"&&thread==GetCurrentThreadId(),
        "Held save observation pinned executable or thread differs");
    Memory memory;const auto game=memory.at<uintptr_t>(image,0x13df8f0);
    const auto map=memory.at<uintptr_t>(image,0x13df8c0);
    const auto random=memory.at<uintptr_t>(image,0x164b250);
    native::SimulationState sim;require(native::read_simulation_state(sim,error),error);
    if(!initial)require(sim.clock.paused&&sim.clock.time_scale==0,"Held save observation requires paused native Clock");
    int32_t saves=-1;const bool saves_read=native::read_saves_in_progress(saves,error);
    require(saves_read&&saves==0,
        "Held save observation requires idle save count="+std::to_string(saves)+" cause="+error);
    const auto words=memory.at<std::array<uint64_t,4>>(random);const auto extra=memory.at<int32_t>(random,0x20);
    mt2fw::mt2::determinism::WorkWitness work([&](uintptr_t p,size_t count){
        std::vector<uint8_t> bytes(count);require(memory.copy(p,bytes.data(),count,error),error);return bytes;
    },image,game);
    auto developers=work.developers(initial).at("comparable");auto& staff=developers.at("developers");
    std::sort(staff.begin(),staff.end(),[](const Json& a,const Json& b){return a.at("id").get<int32_t>()<b.at("id").get<int32_t>();});
    auto subscribers=work.subscribers().at("comparable");auto& subscribers_rows=subscribers.at("rows");
    std::sort(subscribers_rows.begin(),subscribers_rows.end(),[](const Json& a,const Json& b){return a.at("saved_id").get<int32_t>()<b.at("saved_id").get<int32_t>();});
    auto clock=work.clock().at("comparable");clock.erase("panel_present");
    const auto field=[&](uintptr_t p,size_t offset,auto& value,std::string&){value=memory.at<std::remove_reference_t<decltype(value)>>(p,offset);return true;};
    region_saved_identity::Index index;require(region_saved_identity::capture(image,map,map+0x1c0,index,field,error),error);
    region_rng_snapshot::Manifest region_rng;require(region_rng_capture::capture(image,map,region_rng,field,error),error);
    Json npcs=Json::array();
    std::sort(index.saved.begin(),index.saved.end());
    for(const auto& [id,region]:index.saved){
        npc_census::State census;require(npc_census::read(memory,image,region,census,error),error);
        Json rows=Json::array();
        std::sort(census.rows.begin(),census.rows.end(),[](const auto& a,const auto& b){return a.local_uid<b.local_uid;});
        for(const auto& row:census.rows){std::array<uint32_t,10> pose{};size_t slot=0;
            for(float f:row.rotation)pose[slot++]=std::bit_cast<uint32_t>(f);
            for(float f:row.position)pose[slot++]=std::bit_cast<uint32_t>(f);
            for(float f:row.scale)pose[slot++]=std::bit_cast<uint32_t>(f);
            rows.push_back({{"uid",row.local_uid},{"tag",row.local_tag},{"type",row.type},
                {"character_slot",row.character_slot},{"character_type",row.character_type},{"level",row.level},
                {"quest_count",row.quest_count},{"name",row.name},{"pose_bits",pose}});
            if(shared){std::string key;require(npc_probe::action_runtime::local_key(row.local_pointer,row.local_uid,key,error),error);
                require(!key.empty(),"Snapshot NPC canonical identity is empty");rows.back().erase("uid");rows.back()["key"]=key;
                Json quests;require(npc_probe::snapshot_quests(image,id,row,quests,error),error);rows.back()["quests"]=std::move(quests);}

        }
        if(shared){std::sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){return a.at("key")<b.at("key");});
            for(size_t i=1;i<rows.size();++i)require(rows[i-1].at("key")!=rows[i].at("key"),"Snapshot duplicate canonical NPC identity");}
        npcs.push_back({{"region",id},{"next_local_tag",census.next_local_tag},{"rows",rows}});
    }
    Json canonical={{"default_rng",words},{"default_rng_extra",extra},{"clock",clock},{"ledger",ledger(sim)},
        {"developers",developers},{"subscribers",subscribers},{"logged_in_subscribers",sim.logged_in_subscribers},
        {"finished_tasks",work.finished_tasks().at("comparable")},{"root_npcs",npcs},
        {"regions",region_rng_snapshot::encode(region_rng).at("rows")}};
    if(shared){Json construction;require(snapshot_construction::read(image,map,index,construction,error),error);
        canonical["static_construction"]=std::move(construction);
        canonical["development_project"]=work.development_project().at("comparable");}
    work.finish();native::SimulationState after;require(native::read_simulation_state(after,error),error);
    require(ledger(after)==ledger(sim)&&sim.logged_in_subscribers==after.logged_in_subscribers,
        "Held save observation Ledger or subscriber count changed during pure read");
    require(memory.at<std::array<uint64_t,4>>(random)==words&&memory.at<int32_t>(random,0x20)==extra,
        "Held save observation DefaultRNG changed during pure read");
    require(region_rng_capture::unchanged(image,map,region_rng,field,error),error);
    auto gaps=work.gaps();
    if(shared)gaps.push_back("Current development project, work/bug counters and Software version/rating/fun are captured; full technology/subsystem/bug/release graphs and Developer job targets/paths remain outside this projection");
    gaps.push_back(shared?"Root NPC Quest records and task status are captured; AI, combat, path task payloads and Dungeon NPCs are not captured":
        "Root NPC basic rows only; Quest content, AI, combat and Dungeon NPCs are not captured");
    if(shared)gaps.push_back("Construction witness covers saved Region properties, scalar styles, scenery content, terrain heights/contours, root Building identity/definition/pose and mixed Path geometry/topology only; network, zones, Dungeon interiors, Building services/logic, extra grid members, Path record settings and complete software state remain outside it");
    else gaps.push_back("World object graph, terrain, buildings, paths and complete software state are not captured");
    return {{"canonical",canonical},{"complete_world_state",false},{"gaps",gaps},
        {"local_evidence",{{"game",game},{"map",map},{"random",random},{"thread",thread},{"saves",saves}}}};
}
inline bool covered_witness(bool initial,std::array<uint8_t,32>& digest,Json& out,std::string& error)try{
    out=read(true,initial);out.erase("local_evidence");out["schema"]="mt2-snapshot-covered-witness-v3";
    const auto bytes=out.dump();require(bytes.size()<=ULONG_MAX,"Snapshot witness exceeds hash byte capacity");
    BCRYPT_ALG_HANDLE algorithm=nullptr;auto status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    require(status>=0,"Snapshot witness SHA256 provider NTSTATUS="+std::to_string(status));
    status=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())),ULONG(bytes.size()),digest.data(),ULONG(digest.size()));
    const auto closed=BCryptCloseAlgorithmProvider(algorithm,0);
    require(status>=0&&closed>=0,"Snapshot witness SHA256 failed NTSTATUS="+std::to_string(status)+" close="+std::to_string(closed));
    error.clear();return true;
}catch(const std::exception& e){error=std::string("Snapshot covered witness: ")+e.what();return false;}
inline bool shared_witness(std::array<uint8_t,32>& digest,Json& out,std::string& error){
    return covered_witness(false,digest,out,error);
}
// The caller supplies the exact Framework-held zero-step proof before and after.
inline bool initial_witness(std::array<uint8_t,32>& digest,Json& out,std::string& error){
    return covered_witness(true,digest,out,error);
}
inline Json compare(const Json& before,const Json& after){
    Json changed=Json::array();
    for(auto it=before.at("canonical").begin();it!=before.at("canonical").end();++it)
        if(!after.at("canonical").contains(it.key())||it.value()!=after.at("canonical").at(it.key()))changed.push_back(it.key());
    if(before.at("local_evidence")!=after.at("local_evidence"))changed.push_back("local_owners");
    return changed;
}
}
