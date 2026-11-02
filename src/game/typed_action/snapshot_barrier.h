#pragma once
#include "tick_wire.h"

namespace mt2mp::action_session::tick_commands {
struct SnapshotBarrier {
    enum class Phase {Idle,Armed,Sealed,Capturing,Captured,Releasing,Released,Failed};
    Context context;std::string nonce,cause;uint64_t tick=0,order=0;
    SnapshotPurpose purpose=SnapshotPurpose::Ordinary;
    Phase phase=Phase::Idle;std::array<uint8_t,32> witness{};
    uint8_t prepared=0,released=0;bool file_confirmed=false;
    std::array<std::optional<wire::SnapshotControl>,5> peer_prepared{},peer_released{};
    std::optional<wire::SnapshotControl> release;
    static uint8_t bit(uint64_t peer){return uint8_t(1u<<(peer-1));}
    bool host()const{return context.role==Role::Host;}
    bool active()const{return phase!=Phase::Idle&&phase!=Phase::Released;}
    bool fail(const std::string& why,std::string& error){if(cause.empty())cause=why;phase=Phase::Failed;error=cause;return false;}
    bool arm(const Context& owner,const SnapshotRequest& request,uint64_t at,uint64_t assigned,std::string& error){
        if(active()||request.peer!=owner.host_peer||!at||!assigned||request.nonce.size()!=36||uint8_t(request.purpose)>1)
            return fail("Snapshot barrier ARM identity or lifecycle differs",error);
        *this={};context=owner;nonce=request.nonce;purpose=request.purpose;tick=at;order=assigned;phase=Phase::Armed;
        released=bit(context.local_peer);return true;
    }
    bool same(const wire::SnapshotControl& value)const{
        return value.session==context.session&&value.snapshot_uuid==context.snapshot_uuid&&value.nonce==nonce&&value.tick==tick&&value.order==order;
    }
    wire::SnapshotControl packet(wire::SnapshotPhase kind,uint8_t ack)const{
        return {context.session,tick,order,context.snapshot_uuid,nonce,kind,ack,witness};
    }
    bool seal(const std::array<uint8_t,32>& hash,std::string& error){
        if(phase!=Phase::Armed||hash==std::array<uint8_t,32>{})return fail("Snapshot barrier SEAL lifecycle or hash differs",error);
        witness=hash;prepared=bit(context.local_peer);
        for(uint64_t peer=1;peer<=4;++peer)if(peer_prepared[peer]){
            if(peer_prepared[peer]->witness!=witness)return fail("Snapshot prepared covered witness differs peer="+std::to_string(peer),error);
            prepared|=bit(peer);
        }
        phase=Phase::Sealed;return true;
    }
    bool receive(uint64_t peer,const wire::SnapshotControl& value,std::string& error){
        if(peer<1||peer>4||peer==context.local_peer||!(context.peer_mask&bit(peer))||!same(value)||phase==Phase::Failed||phase==Phase::Idle)
            return fail("Snapshot control authenticated owner/nonce/tick/order differs",error);
        if(value.phase==wire::SnapshotPhase::Prepared){
            if(!host()||value.ack_mask!=bit(peer))return fail("Snapshot Prepared requires one actual Client ACK",error);
            auto& prior=peer_prepared[peer];if(prior)return *prior==value||fail("Conflicting duplicate Snapshot Prepared",error);
            if(phase!=Phase::Armed&&phase!=Phase::Sealed)return fail("Late Snapshot Prepared phase differs",error);
            if(phase==Phase::Sealed&&value.witness!=witness)return fail("Snapshot prepared covered witness differs peer="+std::to_string(peer),error);
            prior=value;if(phase==Phase::Sealed)prepared|=bit(peer);return true;
        }
        if(value.witness!=witness||value.ack_mask!=context.peer_mask)return fail("Snapshot release covered witness or complete ACK mask differs",error);
        if(value.phase==wire::SnapshotPhase::Release){
            if(host()||peer!=context.host_peer)return fail("Snapshot Release requires actual Host",error);
            if(release)return *release==value||fail("Conflicting duplicate Snapshot Release",error);
            if(phase!=Phase::Sealed)return fail("Snapshot Release arrived before local SEAL",error);
            release=value;phase=Phase::Releasing;return true;
        }
        if(value.phase==wire::SnapshotPhase::Released){
            if(!host())return fail("Snapshot Released requires Client to Host",error);
            auto& prior=peer_released[peer];if(prior)return *prior==value||fail("Conflicting duplicate Snapshot Released",error);
            if(phase!=Phase::Releasing)return fail("Snapshot Released arrived before successful capture/release",error);
            prior=value;released|=bit(peer);return true;
        }
        return fail("Unknown Snapshot control phase",error);
    }
    bool begin_capture(std::string& error){
        if(!host()||phase!=Phase::Sealed||prepared!=context.peer_mask)return fail("Snapshot CAPTURE lacks all equal prepared ACKs",error);
        phase=Phase::Capturing;return true;
    }
    bool capture_returned(bool okay,const std::string& original,std::string& error){
        if(phase!=Phase::Capturing)return fail("Snapshot capture callback lifecycle differs",error);
        if(!okay)return fail(original.empty()?"Snapshot native capture failed without original cause":original,error);
        phase=Phase::Captured;return true;
    }
    bool confirm_file(const std::string& id,std::string& error){
        if(!host()||phase!=Phase::Captured||nonce!=id||file_confirmed)return fail("Snapshot file/companion receipt lifecycle differs",error);
        file_confirmed=true;return true;
    }
    bool begin_release(std::string& error){
        if(!host()||phase!=Phase::Captured||!file_confirmed||prepared!=context.peer_mask)return fail("Snapshot release lacks captured file/companion and all ACKs",error);
        phase=Phase::Releasing;return true;
    }
    bool finish_release(std::string& error){
        if(phase!=Phase::Releasing||(host()&&released!=context.peer_mask)||(!host()&&!release))return fail("Snapshot release acknowledgment incomplete",error);
        phase=Phase::Released;return true;
    }
};
}
