#pragma once
#include "ordered_proof_owner.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/game/normal_load_identity.h"
#include "native_open_runtime.h"
#include "monster_saved_identity_runtime.h"
#include <Windows.h>

namespace mt2mp::game::ordered_session {
template<bool RecoveryLocal=false>
inline bool read_native_proof_impl(MT2FW_MT2_StepProofReply& out,uint32_t& role,std::string& error){
    out={};out.size=sizeof(out);role=MT2FW_MT2_SESSION_UNKNOWN;
    building_mixed_identity::opened_file::ReadOriginTransaction transaction;
    net::OrderedSnapshotIdentity identity;
    if(!(RecoveryLocal?net::read_ordered_snapshot_local(identity,error,transaction):net::read_ordered_snapshot(identity,error,transaction)))return false;
    if(!identity.file_verified||!identity.native_prepared||!identity.native_generation||!identity.seed_present||
       !identity.bytes||identity.snapshot_uuid.empty()||identity.snapshot_uuid.size()>=sizeof(out.snapshot_uuid)||
       identity.sha256.size()!=64||identity.sha256.find_first_not_of("0123456789abcdef")!=std::string::npos||
       identity.local_peer<1||identity.local_peer>4||!identity.members[identity.local_peer]){
        error="Ordered proof requires verified snapshot bytes, agreed seed and current NativeOpen preparation";return false;}
    const auto native=building_mixed_identity::opened_file::status();
    if(native.game_thread!=GetCurrentThreadId()){
        error="Ordered proof requested outside the native game thread";return false;}
    out.thread=native.game_thread;out.session=identity.session;out.generation=identity.native_generation;
    out.snapshot_bytes=identity.bytes;out.agreed_seed=identity.seed;out.seed_present=1;
    out.frozen_peer_mask=identity.peer_mask;
    region_rng_snapshot::Proof regions;
    if(!monster_saved_identity_runtime::read_prepared_region_rng(identity.snapshot_uuid,identity.sha256,identity.bytes,
        identity.region_rng_mode,UINT32_MAX,regions,error))return false;
    out.region_rng_mode=regions.mode;out.region_rng_count=regions.count;out.region_rng_index=UINT32_MAX;
    std::memcpy(out.snapshot_uuid,identity.snapshot_uuid.c_str(),identity.snapshot_uuid.size()+1);
    const auto nibble=[](char value){return value<='9'?value-'0':value-'a'+10;};
    for(size_t i=0;i<32;++i)out.snapshot_sha256[i]=static_cast<unsigned char>(
        (nibble(identity.sha256[2*i])<<4)|nibble(identity.sha256[2*i+1]));
    for(size_t i=0;i<32;++i)out.region_rng_sha256[i]=static_cast<unsigned char>(
        (nibble(regions.sha256[2*i])<<4)|nibble(regions.sha256[2*i+1]));
    role=identity.local_peer==1?MT2FW_MT2_SESSION_HOST:MT2FW_MT2_SESSION_CLIENT;
    out.phase=identity.request_attempted?MT2FW_MT2_STEP_PROOF_LOADING:MT2FW_MT2_STEP_PROOF_PRELOAD;
    if(identity.request_attempted){
        bool normal_confirmed=false;
        if(!normal_load_identity::read_load_phase(identity.sha256,identity.snapshot_uuid,normal_confirmed,error))return false;
        if(identity.loaded&&normal_confirmed){
            building_mixed_identity::opened_file::CurrentWorld current;
            if(!native_open_runtime::read_current_world(current,error,transaction))return false;
            if(current.uuid!=identity.snapshot_uuid||current.sha256!=identity.sha256||current.bytes!=identity.bytes||
               current.generation!=identity.native_generation||current.world_epoch!=identity.world_epoch){
                error="Ordered sealed native world differs from selected snapshot";return false;}
            out.phase=MT2FW_MT2_STEP_PROOF_LOADED;out.world_epoch=current.world_epoch;
            out.game=reinterpret_cast<uintptr_t>(current.game);out.mode=reinterpret_cast<uintptr_t>(current.mode);
            out.state=reinterpret_cast<uintptr_t>(current.state);out.world=reinterpret_cast<uintptr_t>(current.world);
            out.map=reinterpret_cast<uintptr_t>(current.map);
        }
    }
    if(!transaction.finish(error))return false;
    error.clear();return true;
}
inline bool read_native_proof(MT2FW_MT2_StepProofReply& out,uint32_t& role,std::string& error){return read_native_proof_impl<false>(out,role,error);}
inline bool read_recovery_local_proof(MT2FW_MT2_StepProofReply& out,uint32_t& role,std::string& error){return read_native_proof_impl<true>(out,role,error);}
inline bool read_native_region_row(uint32_t index,MT2FW_MT2_StepProofReply& out,std::string& error){
    net::OrderedSnapshotIdentity identity;
    if(!net::read_ordered_snapshot(identity,error))return false;
    region_rng_snapshot::Proof regions;
    if(index==UINT32_MAX||!monster_saved_identity_runtime::read_prepared_region_rng(identity.snapshot_uuid,identity.sha256,
        identity.bytes,identity.region_rng_mode,index,regions,error))return false;
    if(out.region_rng_mode!=regions.mode||out.region_rng_count!=regions.count||out.snapshot_bytes!=identity.bytes||
       out.generation!=identity.native_generation||out.session!=identity.session||std::string(out.snapshot_uuid)!=identity.snapshot_uuid){
        error="Region RNG row callback differs from current frozen native proof";return false;
    }
    const auto nibble=[](char value){return value<='9'?value-'0':value-'a'+10;};
    for(size_t i=0;i<32;++i)if(out.region_rng_sha256[i]!=static_cast<unsigned char>(
        (nibble(regions.sha256[2*i])<<4)|nibble(regions.sha256[2*i+1]))){
        error="Region RNG row commitment changed after metadata proof";return false;
    }
    out.region_rng_index=index;out.region_rng_id=regions.row.region;out.region_rng_extra=regions.row.extra;
    std::copy(regions.row.words.begin(),regions.row.words.end(),std::begin(out.region_rng_words));
    error.clear();return true;
}

}
