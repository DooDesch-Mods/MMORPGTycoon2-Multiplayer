#pragma once
#include "action_session.h"
#include "mt2mp/net/snapshot_sync.h"
#include <native_step_recovery_draft.h>
#include <thread>

namespace mt2mp::action_session::recovery_read {
struct Selection {Context context;uint64_t generation=0,epoch=0;std::thread::id thread;};
inline thread_local const Selection* selected=nullptr;
inline bool scoped_source(net::OrderedSnapshotIdentity& native,std::string& error){
    if(!selected){error="Recovery source requires its active held read scope";return false;}
    if(!net::read_ordered_snapshot_local(native,error))return false;
    const Context actual{native.session,native.local_peer,1,native.peer_mask,native.local_peer==1?Role::Host:Role::Client,native.snapshot_uuid};
    if(selected->thread!=std::this_thread::get_id()||actual!=selected->context||!native.loaded||
       native.native_generation!=selected->generation||native.world_epoch!=selected->epoch){
        error="Recovery read changed its retained Context or loaded native generation/world";return false;
    }
    error.clear();return true;
}
inline bool local(Context& out,std::string& error){
    net::OrderedSnapshotIdentity native;if(!scoped_source(native,error))return false;
    out={native.session,native.local_peer,1,native.peer_mask,native.local_peer==1?Role::Host:Role::Client,native.snapshot_uuid};
    error.clear();return true;
}
inline bool source(net::OrderedSnapshotIdentity& native,std::string& error){
    return selected?scoped_source(native,error):net::read_ordered_snapshot(native,error);
}
inline bool snapshot_current(const std::string& snapshot,uint64_t generation,uint64_t epoch,std::string& error){
    if(!selected){
        const auto actual=net::current_snapshot_id();const auto ordinary_epoch=net::current_world_epoch();
        if(actual!=snapshot||!ordinary_epoch){error="ordinary snapshot identity/epoch differs: actual_uuid="+actual+" required_uuid="+snapshot+" ordinary_epoch="+std::to_string(ordinary_epoch);return false;}
        error.clear();return true;
    }
    net::OrderedSnapshotIdentity native;std::string cause;
    if(!net::read_ordered_snapshot_local(native,cause)){error="scoped local snapshot proof failed: "+cause;return false;}
    Context actual{native.session,native.local_peer,1,native.peer_mask,native.local_peer==1?Role::Host:Role::Client,native.snapshot_uuid};
    if(selected->thread!=std::this_thread::get_id()||actual!=selected->context||!native.loaded||!native.native_generation||!native.world_epoch||
       native.snapshot_uuid!=snapshot||native.native_generation!=generation||native.world_epoch!=epoch||
       native.native_generation!=selected->generation||native.world_epoch!=selected->epoch){
        error="scoped snapshot proof differs: actual_uuid="+native.snapshot_uuid+" required_uuid="+snapshot+
            " loaded="+std::to_string(native.loaded)+" actual_generation="+std::to_string(native.native_generation)+
            " required_generation="+std::to_string(generation)+" actual_epoch="+std::to_string(native.world_epoch)+
            " required_epoch="+std::to_string(epoch)+" owner_thread="+std::to_string(selected->thread==std::this_thread::get_id())+
            " context_match="+std::to_string(actual==selected->context);return false;
    }
    error.clear();return true;
}
// Only synchronous census/capture/companion callbacks construct this scope.
// It neither enters ordered execution nor changes any Controller admission.
class Scope {
public:
    Scope(const Context& expected,const MT2FW_MT2_RecoveryReceipt& receipt,std::string& error){
        if(selected||receipt.size!=sizeof(receipt)||!receipt.interruption||receipt.session!=expected.session||
           receipt.local_slot!=expected.local_peer||receipt.original_mask!=expected.peer_mask||receipt.workers_held!=1||
           receipt.phase<MT2FW_MT2_RECOVERY_PREPARED||receipt.phase>MT2FW_MT2_RECOVERY_PUBLISHED){
            error="Recovery read requires one prepared held native callback";return;
        }
        net::OrderedSnapshotIdentity native;
        if(!net::read_ordered_snapshot_local(native,error))return;
        if(!native.loaded||!native.native_generation||!native.world_epoch||native.native_generation!=receipt.generation||native.world_epoch!=receipt.world_epoch||native.session!=expected.session||
           native.local_peer!=expected.local_peer||native.peer_mask!=expected.peer_mask||native.snapshot_uuid!=expected.snapshot_uuid){
            error="Recovery read differs from its current NativeOpen generation/world";return;
        }
        selection_={expected,receipt.generation,receipt.world_epoch,std::this_thread::get_id()};selected=&selection_;active_=true;error.clear();
    }
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
    ~Scope(){if(active_)selected=nullptr;}
    explicit operator bool()const{return active_;}
private:
    Selection selection_;bool active_=false;
};
}
