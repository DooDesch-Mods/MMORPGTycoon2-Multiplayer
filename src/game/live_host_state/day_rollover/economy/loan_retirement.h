#pragma once
#include "monetary.h"
#include <algorithm>
#include <set>

namespace mt2mp::game::live_host_state::day::economy {
// Native NewDay reduces principal by the fixed daily payment, clamps at zero,
// and removes that allocation. Cash is supplied by the authoritative Day Frame.
inline bool loan_retirement_plan(const State& before,const State& target,
                                std::vector<size_t>& removed,std::string& e){
    removed.clear();
    const auto& a=before.day.frame;const auto& b=target.day.frame;
    if(a.session!=b.session||a.snapshot!=b.snapshot||a.epoch!=b.epoch||
       a.source_generation!=b.source_generation||before.day.history.empty()||target.day.history.empty()){
        e="Loan retirement exact Day origin/history differs";return false;
    }
    const auto first=before.day.history.back().sequence,last=target.day.history.back().sequence;
    if(last<=first||last-first>history_limit){
        e="Loan retirement requires newer retained Day lineage";return false;
    }
    const auto days=last-first;std::vector<Loan> survivors;std::set<std::string> keys;
    for(size_t i=0;i<before.loans.size();++i){
        auto row=before.loans[i];
        if(row.text[0].empty()||!keys.insert(row.text[0]).second||row.amounts[0]<0||row.amounts[1]<0){
            e="Loan retirement prior key/principal/payment invalid";return false;
        }
        const auto remaining=std::max<int64_t>(0,int64_t(row.amounts[0])-int64_t(row.amounts[1])*int64_t(days));
        if(!remaining)removed.push_back(i);
        else {row.amounts[0]=int32_t(remaining);survivors.push_back(std::move(row));}
    }
    if(survivors!=target.loans){removed.clear();e="Loan retirement Host rows differ from complete original daily principal/order";return false;}
    if(removed.empty()){e="Loan retirement has no naturally settled allocation";return false;}
    e.clear();return true;
}

// Read after each deleting destructor. No pointer receipt crosses that boundary.
inline bool reconcile_retired_loans(Port& port,Owners& before,const State& target,
                                   Receipt& receipt,std::string& e){
    receipt={};std::vector<size_t> removed;
    if(!loan_retirement_plan(before.state,target,removed,e))return false;
    for(auto it=removed.rbegin();it!=removed.rend();++it){
        const auto index=*it;auto expected=before;
        if(expected.loans.size()!=expected.state.loans.size()||index>=expected.loans.size()){
            e="Loan retirement actual allocation extent differs";receipt.unknown=receipt.attempted;return false;
        }
        expected.loans.erase(expected.loans.begin()+index);
        expected.state.loans.erase(expected.state.loans.begin()+index);
        Receipt native;const bool okay=port.retire_loan(before,index,native,e);
        receipt.attempted|=native.attempted;receipt.attempted_stores+=native.attempted_stores;receipt.stores+=native.stores;
        if(!okay||!native.attempted||!native.confirmed||native.unknown){
            if(e.empty())e=native.cause.empty()?"Loan native retirement not confirmed":native.cause;
            receipt.unknown=receipt.attempted||native.unknown;receipt.cause=e;return false;
        }
        Owners actual;
        if(!port.read(actual,e)||actual.history.pin!=expected.history.pin||actual.history.owners!=expected.history.owners||
           actual.subscriber_manager!=expected.subscriber_manager||actual.subscriber_data!=expected.subscriber_data||
           actual.loan_data!=expected.loan_data||actual.offer_data!=expected.offer_data||actual.subscribers!=expected.subscribers||
           actual.subscriber_slots!=expected.subscriber_slots||actual.needs_owners!=expected.needs_owners||
           actual.loans!=expected.loans||actual.offers!=expected.offers||actual.next_id!=expected.next_id||actual.state!=expected.state){
            if(e.empty())e="Loan retirement full fresh owner/value readback differs";
            receipt.unknown=true;receipt.cause=e;return false;
        }
        before=std::move(actual);
    }
    if(before.state.offers!=target.offers){
        auto expected=before;expected.state.offers=target.offers;Receipt native;
        const bool okay=port.refresh_loan_offers(before,native,e);
        receipt.attempted|=native.attempted;receipt.attempted_stores+=native.attempted_stores;receipt.stores+=native.stores;
        if(!okay||!native.attempted||!native.confirmed||native.unknown){
            if(e.empty())e=native.cause.empty()?"Loan original Offer rebuild not confirmed":native.cause;
            receipt.unknown=receipt.attempted||native.unknown;receipt.cause=e;return false;
        }
        Owners actual;
        if(!port.read(actual,e)||actual.history.pin!=expected.history.pin||actual.history.owners!=expected.history.owners||
           actual.subscriber_manager!=expected.subscriber_manager||actual.subscriber_data!=expected.subscriber_data||
           actual.loan_data!=expected.loan_data||actual.subscribers!=expected.subscribers||
           actual.subscriber_slots!=expected.subscriber_slots||actual.needs_owners!=expected.needs_owners||
           actual.loans!=expected.loans||actual.offers.size()!=target.offers.size()||
           actual.next_id!=expected.next_id||actual.state!=expected.state){
            if(e.empty())e="Loan original Offer rebuild full fresh owner/value readback differs";
            receipt.unknown=true;receipt.cause=e;return false;
        }
        before=std::move(actual);
    }
    receipt.confirmed=true;e.clear();return true;
}
}
