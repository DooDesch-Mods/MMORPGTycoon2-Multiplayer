namespace {
uint32_t rebuild_loan_offers(uintptr_t image,uintptr_t business){
    __try{reinterpret_cast<void(*)(void*)>(image+0x57300)(reinterpret_cast<void*>(business));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t erase_loan_allocation(uintptr_t business,uintptr_t slots,size_t index,size_t count,
                               uintptr_t owner,uintptr_t destroy,bool& shifted,bool& destroyed){
    __try{
        auto* rows=reinterpret_cast<uintptr_t*>(slots);
        if(*reinterpret_cast<uintptr_t*>(business+0x98)!=slots||
           *reinterpret_cast<int32_t*>(business+0xa0)!=int32_t(count)||rows[index]!=owner)return ERROR_INVALID_DATA;
        if(index+1<count)std::memmove(rows+index,rows+index+1,(count-index-1)*sizeof(uintptr_t));
        *reinterpret_cast<int32_t*>(business+0xa0)=int32_t(count-1);shifted=true;
        reinterpret_cast<void(*)(void*)>(destroy)(reinterpret_cast<void*>(owner));destroyed=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool NativePort::retire_loan(const Owners& before,size_t index,Receipt& receipt,std::string& e){
    receipt={};
    if(index>=before.loans.size()||before.loans.size()!=before.state.loans.size()||before.loans.size()>1024)
        return fail(e,"Loan retirement exact current row/index differs");
    if(!current(before,before.loans[index],false,e))return false;
    Owners actual;if(!read(actual,e)||actual.history.pin!=before.history.pin||actual.history.owners!=before.history.owners||
       actual.state!=before.state||actual.loans!=before.loans||actual.loan_data!=before.loan_data||
       actual.subscribers!=before.subscribers||actual.subscriber_slots!=before.subscriber_slots||
       actual.needs_owners!=before.needs_owners||actual.offers!=before.offers||actual.next_id!=before.next_id)
        return fail(e,e.empty()?"Loan retirement fresh full prior differs":e);
    const auto image=before.history.pin.image;uintptr_t destroy=0;
    std::vector<unsigned char> body(144);
    if(!ap(before.loans[index],image,0x17438e0,e)||!at(image+0x17438e0,8,destroy,e)||destroy!=image+0xc35400||
       !memory.copy(destroy,body.data(),body.size(),e)||
       mt2mp::sha256(body)!="8c5f1d1d535f135687cc2c0892f661d69db40c9b7a2b042d852b2dd0c09559ca")
        return fail(e,e.empty()?"Loan retirement exact original deleting destructor differs":e);
    bool shifted=false,destroyed=false;receipt.attempted=true;++receipt.attempted_stores;
    live_host_state::apply_timing::PageReadScope callback(live_host_state::apply_timing::PageBoundary::OriginalCall);
    const auto code=erase_loan_allocation(before.history.pin.business,before.loan_data,index,before.loans.size(),
                                          before.loans[index],destroy,shifted,destroyed);
    if(shifted)++receipt.stores;
    if(code||!shifted||!destroyed){receipt.unknown=true;receipt.cause="Loan native array erase/deleting destructor Unknown SEH="+std::to_string(code)+"; no replay";return fail(e,receipt.cause);}
    receipt.confirmed=true;e.clear();return true;
}
bool NativePort::refresh_loan_offers(const Owners& before,Receipt& receipt,std::string& e){
    receipt={};if(!current(before,0,false,e))return false;
    Owners actual;if(!read(actual,e)||actual.history.pin!=before.history.pin||actual.history.owners!=before.history.owners||
       actual.state!=before.state||actual.loans!=before.loans||actual.loan_data!=before.loan_data||
       actual.offer_data!=before.offer_data||actual.offers!=before.offers||actual.subscribers!=before.subscribers||
       actual.subscriber_slots!=before.subscriber_slots||actual.needs_owners!=before.needs_owners||actual.next_id!=before.next_id)
        return fail(e,e.empty()?"Loan Offer refresh fresh full prior differs":e);
    const auto image=before.history.pin.image;std::vector<unsigned char> body(0x270);
    if(!memory.copy(image+0x57300,body.data(),body.size(),e)||
       mt2mp::sha256(body)!="3d8bcfc5d3cf79b6bf49c6f1c3206c97cc2e9f5b0add76084ed439bf327b54bf")
        return fail(e,e.empty()?"Loan original SetLoanOffers whole body differs":e);
    receipt.attempted=true;++receipt.attempted_stores;
    live_host_state::apply_timing::PageReadScope callback(live_host_state::apply_timing::PageBoundary::OriginalCall);
    const auto code=rebuild_loan_offers(image,before.history.pin.business);
    if(code){receipt.unknown=true;receipt.cause="Loan original SetLoanOffers Unknown SEH="+std::to_string(code)+"; no replay";return fail(e,receipt.cause);}
    ++receipt.stores;receipt.confirmed=true;e.clear();return true;
}
