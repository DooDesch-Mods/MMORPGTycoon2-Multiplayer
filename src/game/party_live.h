#pragma once
#include "party_saved_identity_runtime.h"
#include "party_record.h"
#include <functional>

namespace mt2mp::game::party_live {
using Raw=party_saved_identity_runtime::NativeCensus;
using Member=dungeon_party_census::Member;
struct Calls {uint32_t factory=0,load=0,context=0,toon_leave=0,toon_bind=0,release=0,remove=0;};
struct Receipt {
    bool attempted=false,returned=false,confirmed=false,existing=false,reserved=false,unknown=false;
    Member member;
    Calls calls;
    std::string cause;
};
struct Reservation {party_identity::Scope scope;std::string key;Member member;};
// This port is local engine control. Record is portable; Raw, Member and Receipt
// retain native addresses only inside the current process.
struct Port {
    virtual ~Port()=default;
    virtual bool batch(const action_session::Context&,const action_session::Binding&,
                       const std::function<bool(std::string&)>&,std::string&)=0;
    virtual bool current(const action_session::Context&,const action_session::Binding&,Raw&,std::string&)=0;
    virtual bool capture(const Member&,const References&,Record&,std::string&)=0;
    virtual bool birth(const Record&,const References&,Member&,Receipt&,std::string&)=0;
    virtual bool adopt(const Member&,const Record&,const References&,Receipt&,std::string&)=0;
    virtual bool remove(const Raw&,const Member&,Receipt&,std::string&)=0;
    virtual bool find(uint64_t,uintptr_t&,std::string&)=0;
    virtual bool reserve(const std::string&,Member&,Receipt&,std::string&)=0;
    virtual bool reserved(const std::string&,Member&,std::string&)=0;
    virtual bool prepared(const std::string&)const=0;
};
bool captured(const subscriber_lifecycle::Graph&,uintptr_t root,const References&,Record&,std::string&);
class Adapter {
    Port& native_;
    party_identity::Scope locked_scope_;
    std::string locked_cause_;
    bool fresh(const action_session::Context&,const action_session::Binding&,Raw&,std::string&);
    bool unknown(const party_identity::Scope&,Receipt&,std::string&);
    bool current_record_held(const action_session::Context&,const action_session::Binding&,const std::string&,
                             const References&,Record&,std::string&);
    bool current_records_held(const action_session::Context&,const action_session::Binding&,const References&,std::vector<Record>&,std::string&);
    bool client_birth_held(const action_session::Context&,const action_session::Binding&,const std::string&,uint32_t,
                           const Record&,const References&,Receipt&,std::string&);
    bool client_retire_held(const action_session::Context&,const action_session::Binding&,const std::string&,Receipt&,std::string&);
    bool client_adopt_held(const action_session::Context&,const action_session::Binding&,const std::string&,const Record&,const References&,Receipt&,std::string&);
public:
    explicit Adapter(Port& native):native_(native){}
    bool raw_current(const action_session::Context&,const action_session::Binding&,Raw&,std::string&);
    bool bind_host_birth(const action_session::Context&,const action_session::Binding&,const Raw& actual_before,
                         const std::string& operation,uint32_t ordinal,const Member&,Receipt&,std::string&);
    bool current_record(const action_session::Context&,const action_session::Binding&,const std::string& key,
                        const References&,Record&,std::string&);
    bool current_records(const action_session::Context&,const action_session::Binding&,
                         const References&,std::vector<Record>&,std::string&);
    bool client_birth(const action_session::Context&,const action_session::Binding&,const std::string& operation,uint32_t ordinal,
                      const Record&,const References&,Receipt&,std::string&);
    bool client_retire(const action_session::Context&,const action_session::Binding&,const std::string& key,Receipt&,std::string&);
    bool client_adopt(const action_session::Context&,const action_session::Binding&,const std::string& key,
                      const Record&,const References&,Receipt&,std::string&);
    bool reserve_birth(const action_session::Context&,const action_session::Binding&,const std::string& operation,uint32_t ordinal,
                       const Record&,Reservation&,Receipt&,std::string&);
    bool resolve_reserved(const action_session::Context&,const action_session::Binding&,const std::string& key,
                          uintptr_t&,std::string&);
    bool host_retired(const action_session::Context&,const action_session::Binding&,const std::string& key,
                      const party_identity::Entry& before,Receipt&,std::string&);
};
}
