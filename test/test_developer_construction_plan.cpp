#include "developer_staff/construction_plan.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::developer_staff::construction;
namespace {
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void apply(std::vector<int32_t>& in,std::vector<int32_t>& out,const Membership& op){
    auto& destination=op.outside?out:in;
    auto& source=op.reorder?destination:(op.outside?in:out);
    auto found=std::find(source.begin(),source.end(),op.id);
    require(found!=source.end(),"original RemoveDev receiver missing");source.erase(found);
    require(std::find(destination.begin(),destination.end(),op.id)==destination.end(),"original AddDev duplicate");
    destination.push_back(op.id);
}
}
int main(){try{
    struct Pair{std::vector<int32_t> in,out;};std::vector<Pair> states;
    std::vector<int32_t> permutation{0,1,2,3};
    do{for(size_t split=0;split<=permutation.size();++split)
        states.push_back({{permutation.begin(),permutation.begin()+static_cast<ptrdiff_t>(split)},
                          {permutation.begin()+static_cast<ptrdiff_t>(split),permutation.end()}});
    }while(std::next_permutation(permutation.begin(),permutation.end()));
    size_t cases=0;std::vector<Membership> plan;std::string error;
    for(const auto& before:states)for(const auto& target:states){auto in=before.in,out=before.out;
        require(membership_plan(in,out,target.in,target.out,plan,error),"valid native manager plan rejected");
        for(const auto& op:plan)apply(in,out,op);
        require(in==target.in&&out==target.out,"original manager operations did not reach ordered target");++cases;}
    require(membership_plan({0,1,2,3},{},{1,2,3,0},{},plan,error),"skipped completed trip rejected");
    require(plan.size()==1&&plan[0].id==0&&plan[0].reorder,"skipped trip touched unrelated actors");++cases;
    require(!membership_plan({0,0},{},{0},{},plan,error)&&!error.empty(),"duplicate current actor accepted");++cases;
    require(!membership_plan({0},{},{0},{0},plan,error)&&!error.empty(),"duplicate target actor accepted");++cases;
    require(!membership_plan({0},{},{1},{},plan,error)&&!error.empty(),"different actor set accepted");++cases;
    std::vector<JobState> actual{{"action:a",true,false,0},{"action:b",false,true,-1}},expanded;
    std::vector<LocalJob> links{{"action:a",1,1},{"action:b",2,2}};
    require(expand_pending(actual,{},links,expanded,error)&&expanded==std::vector<JobState>{{"action:a",false,true,-1},{"action:b",false,true,-1}},"empty pending frame did not complete known occurrences");++cases;
    require(expand_pending(actual,{{"action:a",true,false,0}},links,expanded,error)&&expanded[0]==actual[0]&&expanded[1]==actual[1],"pending selected occurrence changed");++cases;
    require(!expand_pending(actual,{{"action:unknown",true,false,-1}},links,expanded,error)&&expanded.empty(),"unknown pending occurrence accepted");++cases;
    require(!expand_pending(actual,{{"action:a",true,false,0},{"action:a",true,false,0}},links,expanded,error),"duplicate pending occurrence accepted");++cases;
    require(!expand_pending(actual,{{"action:a",false,true,-1}},links,expanded,error),"completed rows accumulated on wire");++cases;
    links[0].target_alive=false;
    require(expand_pending(actual,{},links,expanded,error)&&!expanded[0].active&&!expanded[0].present&&expanded[1].active,"removed target was activated by omitted pending row");++cases;
    require(!expand_pending(actual,{},std::vector<LocalJob>{},expanded,error)&&!error.empty(),"missing local occurrence list accepted");++cases;
    links[0].building_key="action:other";
    require(!expand_pending(actual,{},links,expanded,error)&&expanded.empty(),"local occurrence order changed");++cases;
    links[0].building_key="action:a";links[0].target_alive=true;links[0].target_restored=true;
    require(expand_pending(actual,{{"action:a",false,false,-1}},links,expanded,error)&&!expanded[0].present&&!expanded[0].active,"restored inactive receipt became activated");++cases;
    require(!expand_pending(actual,{},links,expanded,error)&&expanded.empty(),"missing restored inactive receipt became activation");++cases;
    links[0].target_restored=false;
    require(!expand_pending(actual,{{"action:a",false,false,-1}},links,expanded,error),"inactive absence accepted without original Redo");++cases;
    links[0].target_restored=true;links[0].target_alive=false;
    require(!expand_pending(actual,{{"action:a",false,false,-1}},links,expanded,error),"removed target accepted as restored live Building");++cases;
    std::cout<<"PASS "<<cases<<" original ordered manager transition cases\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
