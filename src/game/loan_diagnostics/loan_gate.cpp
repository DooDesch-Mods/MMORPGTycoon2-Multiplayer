#include "loan_gate.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace mt2mp::game::loan_diagnostics {
namespace {
bool fail(std::string& e,const std::string& why){e=why;return false;}
bool uuid(const std::string& s){if(s.size()!=36)return false;for(size_t i=0;i<s.size();++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;}return true;}
bool add(int32_t& slot,int64_t delta,std::string& e){const auto v=int64_t(slot)+delta;if(v<INT32_MIN||v>INT32_MAX)return fail(e,"loan native int32 effect overflow");slot=int32_t(v);return true;}
bool common(const Survey& a,const Survey& b){return a.image==b.image&&a.world==b.world&&a.business==b.business&&a.record==b.record&&a.analytics==b.analytics&&a.thread==b.thread&&a.epoch==b.epoch&&a.generation==b.generation&&a.native_origin==b.native_origin&&a.snapshot==b.snapshot&&a.host==b.host&&a.initialized==b.initialized&&a.started==b.started&&a.pending==b.pending&&a.locked==b.locked&&a.live==b.live&&a.scale==b.scale&&a.pause==b.pause;}
json row(const Loan& v){return {{"text",v.text},{"amounts",v.amounts}};}
json row(const Quote& v){return {{"text",v.offer.text},{"amounts",v.offer.amounts},{"eligibility",v.eligibility},{"available",v.available}};}
bool exact_row(Operation op,const json& v){
    const bool offer=op==Operation::TakeFixture;if(!v.is_object()||v.size()!=(offer?4u:2u)||!v.contains("text")||!v.contains("amounts")||!v.at("text").is_array()||v.at("text").size()!=3||!v.at("amounts").is_array()||v.at("amounts").size()!=(offer?3u:4u))return false;
    for(size_t i=0;i<3;++i){const auto& s=v.at("text")[i];if(!s.is_string())return false;const auto& text=s.get_ref<const std::string&>();if((!i&&text.empty())||text.size()>(i?2048u:256u)||text.find('\0')!=std::string::npos)return false;}
    const auto integer=[](const json& x){return x.is_number_integer()&&!x.is_boolean()&&(x.is_number_unsigned()?x.get<uint64_t>()<=uint64_t(INT32_MAX):(x.get<int64_t>()>=INT32_MIN&&x.get<int64_t>()<=INT32_MAX));};
    for(const auto& x:v.at("amounts"))if(!integer(x))return false;
    return !offer||(v.contains("eligibility")&&integer(v.at("eligibility"))&&v.contains("available")&&v.at("available").is_boolean());
}
bool state_valid(const Survey& s,std::string& e){
    if(!uuid(s.native_origin)||!s.epoch||!s.generation||!s.image||!s.world||!s.business||!s.record||!s.thread||!s.analytics||s.ledger.record_present!=1||s.ledger.cash!=s.ledger.record_cash||s.loans.size()!=s.loan_owners.size()||s.offers.size()!=s.offer_owners.size()||s.loans.size()>1024||s.offers.size()>1024||!std::isfinite(s.scale)||!std::isfinite(s.pause))return fail(e,"loan actual world/ledger/owner shape invalid");
    std::map<std::string,bool> ids;
    for(const auto& loan:s.loans){if(loan.text[0].empty()||!ids.emplace(loan.text[0],true).second)return fail(e,"loan actual duplicate/empty Loan ID");}
    ids.clear();for(const auto& q:s.offers)if(q.offer.text[0].empty()||!ids.emplace(q.offer.text[0],true).second)return fail(e,"loan actual duplicate/empty Offer ID");
    return true;
}
}
json public_survey(const Survey& s){json loans=json::array(),offers=json::array();for(const auto& v:s.loans)loans.push_back(row(v));for(const auto& v:s.offers)offers.push_back(row(v));const auto& l=s.ledger;return {{"native_origin_uuid",s.native_origin},{"snapshot_uuid",s.snapshot},{"native_epoch",s.epoch},{"native_generation",s.generation},{"host",s.host},{"controller_initialized",s.initialized},{"controller_started",s.started},{"controller_pending",s.pending},{"controller_locked",s.locked},{"live_state_active",s.live},{"native_scale",s.scale},{"native_pause",s.pause},{"loans",loans},{"offers",offers},{"ledger",{{"cash",l.cash},{"flat_day",l.flat_day},{"record_present",l.record_present},{"record_day",l.record_day},{"record_cash",l.record_cash},{"record_income",l.record_income},{"record_expense",l.record_expense},{"line_counts",l.line_counts},{"line_income",l.line_income}}}};}
bool expected_effect(Operation op,const Survey& before,size_t index,Survey& after,std::string& e){
    if(!state_valid(before,e))return false;
    after=before;auto& l=after.ledger;
    if(op==Operation::TakeFixture){
        if(index>=before.offers.size())return fail(e,"loan owned Offer index absent");
        const auto& q=before.offers[index];const auto& a=q.offer.amounts;
        if(!q.available||a[0]<=0||a[1]<a[0]||a[2]<=0)return fail(e,"loan Offer unavailable or finite principal/duration contract differs");
        if(std::any_of(before.loans.begin(),before.loans.end(),[&](const auto& v){return v.text[0]==q.offer.text[0];}))return fail(e,"loan Offer ID already belongs to a Loan");
        Loan created;created.text=q.offer.text;created.amounts={a[0],a[0]/a[2],int32_t((int64_t(a[1])-a[0])/a[2]),a[2]};
        if(created.amounts[1]<=0)return fail(e,"loan Offer cannot produce a nonempty principal payment");
        if(!add(l.cash,a[0],e)||!add(l.record_cash,a[0],e)||!add(l.record_income,a[0],e)||!add(l.line_counts[17],1,e)||!add(l.line_income[17],a[0],e))return false;
        after.loans.push_back(created);after.offers.erase(after.offers.begin()+index);return true;
    }
    if(index>=before.loans.size())return fail(e,"loan owned Loan index absent");
    const auto& a=before.loans[index].amounts;const int64_t cost=int64_t(a[1])+a[2];
    if(a[1]<=0||a[2]<0||a[0]<=a[1]||cost>before.ledger.cash)return fail(e,"loan payment requires positive installment, retained Loan and sufficient cash");
    after.loans[index].amounts[0]-=a[1];
    if(!add(l.cash,-cost,e)||!add(l.record_cash,-cost,e)||!add(l.record_expense,cost,e)||!add(l.line_counts[15],1,e)||!add(l.line_income[15],-int64_t(a[1]),e))return false;
    if(a[2]&&(!add(l.line_counts[16],1,e)||!add(l.line_income[16],-int64_t(a[2]),e)))return false;
    return true;
}
bool verify_effect(Operation op,const Survey& before,size_t index,const Survey& actual,std::string& e){
    Survey expected;if(!expected_effect(op,before,index,expected,e)||!state_valid(actual,e))return false;
    if(!common(before,actual))return fail(e,"loan native world/owner/session changed after original");
    if(expected.ledger!=actual.ledger)return fail(e,"loan native full Ledger differs after original");
    if(expected.loans!=actual.loans||expected.offers!=actual.offers)return fail(e,"loan native Loan/Offer content or membership differs after original");
    if(op==Operation::Payment){if(before.loan_owners!=actual.loan_owners||before.offer_owners!=actual.offer_owners)return fail(e,"loan payment changed native owner membership");}
    else {
        if(actual.loan_owners.size()!=before.loan_owners.size()+1||!std::equal(before.loan_owners.begin(),before.loan_owners.end(),actual.loan_owners.begin())||!actual.loan_owners.back()||std::find(before.loan_owners.begin(),before.loan_owners.end(),actual.loan_owners.back())!=before.loan_owners.end())return fail(e,"loan new native allocation ownership differs");
        auto offers=before.offer_owners;offers.erase(offers.begin()+index);if(offers!=actual.offer_owners)return fail(e,"loan consumed Offer ownership differs");
    }
    e.clear();return true;
}
bool Gate::read(Port& port,json& out,std::string& e){Survey a,b;if(!port.read(a,e)||!port.read(b,e)||!state_valid(a,e)){out={{"observed",false},{"cause",e}};return false;}if(a!=b){out={{"observed",false}};return fail(e,"loan read roots/catalogue changed during double census");}out=public_survey(a);out["observed"]=true;out["locked"]=locked_;out["receipts"]=json::object();for(const auto& [token,v]:receipts_)out["receipts"][token]=v.second;e.clear();return true;}
bool Gate::run(Port& port,Operation op,const json& args,json& out,std::string& e){
    out={{"attempted",false},{"returned",false},{"confirmed",false},{"unknown",false},{"seh_code",0}};
    const auto binding=op==Operation::TakeFixture?"native_origin_uuid":"snapshot_uuid";
    if(!args.is_object()||args.size()!=3||!args.contains("token")||!args.contains(binding)||!args.contains("expected")||!args.at("token").is_string()||!args.at(binding).is_string()||!uuid(args.at("token").get<std::string>())||!uuid(args.at(binding).get<std::string>())||!exact_row(op,args.at("expected")))return fail(e,"loan mutation requires exact canonical token/binding/expected fields");
    const auto token=args.at("token").get<std::string>();out["token"]=token;out["operation"]=op==Operation::TakeFixture?"take_fixture":"existing_loan_payment";
    const json identity={{"operation",int(op)},{"args",args}};
    if(const auto it=receipts_.find(token);it!=receipts_.end()){out=it->second.second;return fail(e,it->second.first==identity?"loan token already consumed; inspect retained receipt, no replay":"loan token payload conflict; no replay");}
    if(locked_)return fail(e,"loan diagnostic locked; original="+lock_cause_+"; no replay");
    if(busy_||receipts_.size()>=64)return fail(e,"loan diagnostic busy or token history full; no replay");
    Survey before,current,predicted;size_t index=0;
    if(!port.read(before,e)||!state_valid(before,e))return false;
    if(!before.host||before.locked||before.pending||before.scale!=0)return fail(e,"loan mutation requires idle actual Host and native Clock0");
    if(op==Operation::TakeFixture){if(before.initialized||before.started||before.live)return fail(e,"loan membership create requires pre-controller fixture; existing Economy1 cannot replicate new Loan/consumed Offer");if(args.at(binding)!=before.native_origin)return fail(e,"loan fixture actual native origin differs");}
    else {if(!before.initialized||!before.started||!before.live||args.at(binding)!=before.snapshot)return fail(e,"loan payment requires actual live shared Host binding");}
    bool found=false;if(op==Operation::TakeFixture){for(size_t i=0;i<before.offers.size();++i)if(row(before.offers[i])==args.at("expected")){index=i;found=true;}}
    else {for(size_t i=0;i<before.loans.size();++i)if(row(before.loans[i])==args.at("expected")){index=i;found=true;}}
    if(!found)return fail(e,"loan expected full native row absent or changed");
    if(!expected_effect(op,before,index,predicted,e)||!port.read(current,e))return false;
    if(current!=before)return fail(e,"loan actual owner/content changed before invocation");
    busy_=true;receipts_.emplace(token,std::make_pair(identity,out));NativeResult native;
    const bool invoked=port.invoke(op,before,index,native,e);busy_=false;
    out["attempted"]=native.attempted;out["returned"]=native.returned;out["seh_code"]=native.seh;out["native_accepted"]=native.accepted;out["before"]=public_survey(before);
    if(!native.cause.empty())e=native.cause;
    Survey after;const bool read_after=native.attempted&&port.read(after,native.cause);if(read_after)out["after"]=public_survey(after);
    const bool okay=invoked&&native.attempted&&native.returned&&native.accepted&&!native.seh&&e.empty()&&read_after&&verify_effect(op,before,index,after,e);
    if(!okay){if(e.empty())e=!native.cause.empty()?native.cause:"loan original did not produce a confirmed native receipt";locked_=native.attempted;if(locked_)lock_cause_=e;out["unknown"]=native.attempted;}
    out["confirmed"]=okay;out["cause"]=e;receipts_[token].second=out;return okay;
}
}
