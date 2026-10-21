namespace loan_retirement_cases {
using namespace mt2mp::game::live_host_state::day::economy;
Loan debt(const std::string& key,int32_t principal,int32_t payment){return {{key,"display "+key,"description "+key},{principal,payment,7,30}};}
State before(){State value;auto& f=value.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";f.epoch=7;f.source_generation=9;
    value.day.history.push_back({4,{}});value.loans={debt("first",100,40),debt("survivor",1000,50),debt("last",10,10)};return value;}
State after(const State& value){auto target=value;target.day.history.push_back({7,{}});target.loans={debt("survivor",850,50)};return target;}
struct Native final:Port {
    Owners actual;std::vector<Offer> rebuilt;size_t calls=0,reads=0;bool fail=false,drift=false,owner_drift=false,unacknowledged=false;
    bool read(Owners& out,std::string&)override{++reads;out=actual;if(drift&&calls)++out.state.day.frame.values.cash;if(owner_drift&&calls)++out.history.pin.world;return true;}
    bool retire_loan(const Owners& prior,size_t index,Receipt& receipt,std::string& e)override{
        ++calls;receipt={};receipt.attempted=true;receipt.attempted_stores=1;
        if(prior.state!=actual.state||prior.loans!=actual.loans){e="fresh prior changed";receipt.unknown=true;return false;}
        if(fail){e="original deleting destructor exception";receipt.unknown=true;return false;}
        actual.loans.erase(actual.loans.begin()+index);actual.state.loans.erase(actual.state.loans.begin()+index);
        receipt.stores=1;receipt.confirmed=!unacknowledged;return true;
    }
    bool refresh_loan_offers(const Owners&,Receipt& receipt,std::string&)override{
        ++calls;receipt={};receipt.attempted=true;receipt.confirmed=true;receipt.attempted_stores=receipt.stores=1;
        actual.state.offers=rebuilt;actual.offers.assign(rebuilt.size(),1234);actual.offer_data=1233;return true;
    }
    bool store_subscriber(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{e="unexpected Subscriber store";return false;}
    bool store_loan(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{e="unexpected Loan scalar store";return false;}
};
void run(){
    const auto base=before(),target=after(base);std::vector<size_t> erased;std::string e;
    need(loan_retirement_plan(base,target,erased,e)&&erased==std::vector<size_t>{0,2},"complete native three-Day Loan projection: "+e);
    auto port=Native{};port.actual.state=base;port.actual.loans={101,102,103};port.actual.loan_data=88;Owners current=port.actual;Receipt receipt;
    need(reconcile_retired_loans(port,current,target,receipt,e)&&receipt.confirmed&&!receipt.unknown&&receipt.stores==2&&port.calls==2&&port.reads==2,"two native Loan retirements/fresh full reads: "+e);
    need(current.loans==std::vector<uintptr_t>{102}&&current.state.loans==std::vector<Loan>{base.loans[1]}&&current.state.day==base.day,"retirement preserved survivor prior values and full Day before scalar apply");
    auto reject=[&](auto mutate,const std::string& name){auto wanted=target;mutate(wanted);std::vector<size_t> plan;std::string cause;need(!loan_retirement_plan(base,wanted,plan,cause)&&!cause.empty(),name);};
    reject([](auto& s){s.day.frame.epoch++;},"foreign Day epoch accepted");
    reject([](auto& s){s.day.history.back().sequence=4;},"same Day deletion accepted");
    reject([](auto& s){s.day.history.back().sequence=15;},"unretained Day gap accepted");
    reject([](auto& s){s.loans[0].text[1]="foreign name";},"Loan native string replacement accepted");
    reject([](auto& s){s.loans[0].amounts[0]=849;},"wrong surviving principal accepted");
    reject([](auto& s){s.loans[0].amounts[1]=51;},"wrong original daily payment accepted");
    reject([](auto& s){s.loans.clear();},"unsettled Loan was retired");
    reject([](auto& s){s.loans.push_back(debt("new",1,1));},"new Loan invented by daily reconciliation");
    auto renewed=target;renewed.offers.push_back({{"first","offer name","offer description"},{100,120,30}});
    auto renewal=Native{};renewal.actual.state=base;renewal.actual.loans={101,102,103};renewal.rebuilt=renewed.offers;current=renewal.actual;
    need(reconcile_retired_loans(renewal,current,renewed,receipt,e)&&receipt.confirmed&&renewal.calls==3&&renewal.reads==3&&current.state.offers==renewed.offers&&current.offers==std::vector<uintptr_t>{1234},"original Offer rebuild/new allocation not adopted: "+e);
    auto wrong=Native{};wrong.actual.state=base;wrong.actual.loans={101,102,103};current=wrong.actual;
    need(!reconcile_retired_loans(wrong,current,renewed,receipt,e)&&receipt.unknown&&wrong.calls==3,"invented Host Offer was accepted without exact original rebuild");
    for(int mode=0;mode<4;++mode){auto failed=Native{};failed.actual.state=base;failed.actual.loans={101,102,103};auto prior=failed.actual;
        failed.fail=mode==0;failed.drift=mode==1;failed.owner_drift=mode==2;failed.unacknowledged=mode==3;
        Receipt result;std::string cause;need(!reconcile_retired_loans(failed,prior,target,result,cause)&&result.unknown&&!result.confirmed&&failed.calls==1&&!cause.empty(),"Loan failed/unacknowledged original or fresh drift continued");
    }
    auto zero=base;zero.loans={debt("zero",0,0)};auto zero_target=zero;zero_target.day.history.push_back({5,{}});zero_target.loans.clear();
    need(loan_retirement_plan(zero,zero_target,erased,e)&&erased==std::vector<size_t>{0},"native zero-principal retirement omitted");
    auto negative=base;negative.loans[0].amounts[1]=-1;
    need(!loan_retirement_plan(negative,target,erased,e),"negative native daily principal payment accepted");
}
}
