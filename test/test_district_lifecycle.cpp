#include "district_lifecycle.h"
#include <iostream>
#include <stdexcept>

namespace fixture {
namespace d=mt2mp::game::district_lifecycle;
namespace s=mt2mp::game::district_saved_identity;
size_t checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr uintptr_t manager=0x10000,first=0x20000,second=0x30000,third=0x40000;
constexpr uint64_t first_uid=0x100000007,second_uid=0x200000008,third_uid=0x300000009;
const std::string digest(64,'a'),operation="11111111-2222-4333-8444-555555555555";
struct Lookup {
    std::map<uint64_t,uintptr_t> rows;
    bool fail_read=false;
    bool operator()(uint64_t uid,uintptr_t& pointer,std::string& error){
        if(fail_read){error="fixture original lookup failed";return false;}
        const auto found=rows.find(uid);pointer=found==rows.end()?0:found->second;return true;
    }
};
s::Pool initial(){return {manager,{{first,first_uid,0,17},{second,second_uid,1,0}}};}
void run(){
    std::string error;Lookup lookup{{{first_uid,first},{second_uid,second}}};
    const s::SavedPool saved{67,initial()};d::Roster before;
    need(d::seed(digest,saved,lookup,before,error),error);
    const auto first_key=before.members[0].key,second_key=before.members[1].key;
    std::vector<s::Current> rows;
    need(d::current(before,saved.pool,lookup,rows,error)&&rows.size()==2,error);
    auto actual=initial();std::swap(actual.records[0],actual.records[1]);
    actual.records[0].child=0;actual.records[1].child=1;
    need(d::current(before,actual,lookup,rows,error)&&rows[0].key==second_key&&rows[1].key==first_key,"current array reorder transferred aliases");
    actual.records.push_back({third,third_uid,2,0});lookup.rows[third_uid]=third;
    need(!d::current(before,actual,lookup,rows,error),"unconfirmed birth hidden by current read");
    d::Change change;
    need(d::reconcile(before,actual,operation,{},lookup,change,error),error);
    need(change.born==std::vector<std::string>{"district:action:"+operation+":0"}&&change.retired.empty(),"birth ordinal/membership differs");
    need(change.after.members[0].key==second_key&&change.after.members[1].key==first_key,"birth reassigned surviving aliases");
    need(d::current(change.after,actual,lookup,rows,error)&&rows[2].key==change.born[0],error);
    const auto grown=change.after;const auto birth_key=change.born[0];
    actual.records.erase(actual.records.begin()+1);lookup.rows.erase(first_uid);
    need(d::reconcile(grown,actual,operation,{},lookup,change,error),error);
    need(change.retired==std::vector<std::string>{first_key}&&change.born.empty(),"retirement lost original alias");
    const auto reduced=change.after;
    actual.records.push_back({first+0x100,first_uid,2,0});lookup.rows[first_uid]=first+0x100;
    need(d::reconcile(reduced,actual,operation,{{first_uid,first_key}},lookup,change,error),error);
    need(change.restored==std::vector<std::string>{first_key}&&change.born.empty(),"history restoration assigned new alias");
    need(change.after.members[2].key==first_key&&change.after.members[1].key==birth_key,"restore changed surviving key");
    const auto stable=change.after;
    auto bad=actual;bad.records[0].uid=second_uid&0xffffffffULL;
    need(!d::current(stable,bad,lookup,rows,error),"current accepted truncated UID");
    bad=actual;bad.records[0].pointer+=8;
    need(!d::reconcile(stable,bad,operation,{},lookup,change,error),"surviving pointer replaced");
    bad=actual;bad.records[1].uid=bad.records[0].uid;
    need(!d::reconcile(stable,bad,operation,{},lookup,change,error),"duplicate UID admitted");
    bad=actual;bad.records[1].pointer=bad.records[0].pointer;
    need(!d::reconcile(stable,bad,operation,{},lookup,change,error),"duplicate allocation admitted");
    bad=actual;bad.records.pop_back();
    need(!d::reconcile(stable,bad,operation,{},lookup,change,error),"retirement accepted still registered UID");
    need(!d::reconcile(reduced,actual,operation,{{first_uid,second_key}},lookup,change,error),"restore reused surviving key");
    need(!d::reconcile(reduced,actual,operation,{{first_uid,"invalid"}},lookup,change,error),"invalid restore key");
    need(!d::reconcile(reduced,actual,operation,{{first_uid,first_key},{0x40000000a,first_key}},lookup,change,error),"missing restore member accepted");
    need(!d::reconcile(stable,actual,operation,{{first_uid,first_key}},lookup,change,error),"survivor mislabeled restoration");
    bad=actual;++bad.manager;
    need(!d::reconcile(stable,bad,operation,{},lookup,change,error),"foreign manager admitted");
    need(!d::reconcile(stable,actual,"invalid",{},lookup,change,error),"invalid operation admitted");
    const auto previous=change.after;lookup.fail_read=true;
    need(!d::reconcile(stable,actual,operation,{},lookup,change,error)&&error=="fixture original lookup failed","upstream lookup cause lost");
    need(change.after==previous,"failed admission partially published roster");
    need(!d::valid_key("district:action:"+operation+":00"),"padded ordinal admitted");
    need(!d::valid_key("district:action:"+operation+":100000"),"out of range ordinal admitted");
    need(d::valid_key("district:action:"+operation+":99999"),"maximum ordinal rejected");
    lookup.fail_read=false;
    d::Roster empty{manager+0x100,68,{}};s::Pool new_pool{empty.manager,{{third,third_uid,0,0}}};
    size_t next_ordinal=5;
    need(d::reconcile(empty,new_pool,operation,{},lookup,change,error,&next_ordinal),error);
    need(next_ordinal==6&&change.born==std::vector<std::string>{"district:action:"+operation+":5"},"complete Region batch reset its shared birth ordinal");
}
}
int main()try{fixture::run();std::cout<<fixture::checks<<" District lifecycle checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
