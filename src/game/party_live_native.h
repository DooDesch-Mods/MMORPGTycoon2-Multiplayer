#pragma once
#include "party_live.h"
#include "subscriber_lifecycle/cohort.h"

namespace mt2mp::game::party_live_native {
// The existing ObjectContext lookup hook dispatches here only while this
// dedicated new-Party context owns the current Mainthread scope.
void set_lookup_hook_ready(bool);
bool lookup_active()noexcept;
void* lookup(void*,int32_t,subscriber_lifecycle::Lookup);
class NativePort final:public party_live::Port {
    action_session::Context context_;
    action_session::Binding binding_;
    party_live::Raw current_;
    uintptr_t image_=0;
    uint32_t thread_=0;
    bool held_=false;
    party_identity::Scope blocked_scope_;
    std::string blocked_cause_;
    std::map<std::string,party_live::Reservation> reservations_;
    struct Prepared {party_identity::Scope scope;party_live::Member member;party_live::Record target;};
    std::map<std::string,Prepared> prepared_;
    std::string batch_unknown_;
    bool ready_to_init(const party_live::Member&,const party_live::Record&,const party_live::References&,std::string&);
    bool finish_membership(const party_live::Member&,const party_live::Record&,const party_live::References&,party_live::Member&,party_live::Receipt&,std::string&);
    bool load_record(const party_live::Member*,const party_live::Record&,const party_live::References&,party_live::Member&,party_live::Receipt&,std::string&,bool defer_init=false);
public:
    bool owner(std::string&);
    bool batch(const action_session::Context&,const action_session::Binding&,
               const std::function<bool(std::string&)>&,std::string&)override;
    bool current(const action_session::Context&,const action_session::Binding&,party_live::Raw&,std::string&)override;
    bool capture(const party_live::Member&,const party_live::References&,party_live::Record&,std::string&)override;
    bool birth(const party_live::Record&,const party_live::References&,party_live::Member&,party_live::Receipt&,std::string&)override;
    bool adopt(const party_live::Member&,const party_live::Record&,const party_live::References&,party_live::Receipt&,std::string&)override;
    bool remove(const party_live::Raw&,const party_live::Member&,party_live::Receipt&,std::string&)override;
    bool find(uint64_t,uintptr_t&,std::string&)override;
    bool reserve(const std::string&,party_live::Member&,party_live::Receipt&,std::string&)override;
    bool reserved(const std::string&,party_live::Member&,std::string&)override;
    bool prepared(const std::string&)const override;
    bool rearm_toon_party(uintptr_t toon,uint64_t uid,int32_t own_saved_party_id,void* own_object_context,party_live::Receipt&,std::string&);
    bool prepare_reserved(const party_live::Record&,const party_live::References&,party_live::Receipt&,std::string&);
    bool prepare_record(const party_live::Member&,const party_live::Record&,const party_live::References&,party_live::Receipt&,std::string&);
};
}
