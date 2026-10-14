#pragma once
#include "typed_action/action_session.h"
#include "live_host_state/day_rollover/economy/monetary.h"
#include <map>
namespace mt2mp::game::loan_diagnostics {
using json=nlohmann::json;
using Ledger=action_session::Ledger;
using Loan=live_host_state::day::economy::Loan;
using Offer=live_host_state::day::economy::Offer;
struct Quote {
    Offer offer;
    int32_t eligibility=0;
    bool available=false;
    bool operator==(const Quote&)const=default;
};
// Addresses stay local and are omitted from every public receipt.
struct Survey {
    uintptr_t image=0,world=0,business=0,record=0,analytics=0;
    uint32_t thread=0;
    uint64_t epoch=0,generation=0;
    std::string native_origin,snapshot;
    bool host=false,initialized=false,started=false,pending=false,locked=false,live=false;
    float scale=0,pause=0;
    Ledger ledger;
    std::vector<Loan> loans;
    std::vector<Quote> offers;
    std::vector<uintptr_t> loan_owners,offer_owners;
    bool operator==(const Survey&)const=default;
};
enum class Operation {TakeFixture,Payment};
struct NativeResult {bool attempted=false,returned=false,accepted=false;uint32_t seh=0;std::string cause;};
struct Port {
    virtual ~Port()=default;
    virtual bool read(Survey&,std::string&)=0;
    virtual bool invoke(Operation,const Survey&,size_t,NativeResult&,std::string&)=0;
};
bool expected_effect(Operation,const Survey&,size_t,Survey&,std::string&);
bool verify_effect(Operation,const Survey&,size_t,const Survey&,std::string&);
json public_survey(const Survey&);
class Gate {
    std::map<std::string,std::pair<json,json>> receipts_;
    bool locked_=false,busy_=false;
    std::string lock_cause_;
public:
    bool read(Port&,json&,std::string&);
    bool run(Port&,Operation,const json&,json&,std::string&);
};
bool observe(json&,std::string&);
bool mutate(Operation,const json&,json&,std::string&);
}
