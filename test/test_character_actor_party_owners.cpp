#include "character_actor_party_owners.h"
#include "character_actor_records.h"
#include <iostream>

namespace aliases=mt2mp::game::character_actor_party_owners;
namespace parties=mt2mp::game::party_saved_identity_runtime;
namespace identity=mt2mp::game::party_identity;
namespace record=mt2mp::subscriber_lifecycle;
namespace complete=mt2mp::game::duplicate_records;
const std::string snapshot="12345678-1234-1234-1234-123456789abc";
const std::string saved_key=std::string(64,'a')+":world:party:7";
const identity::Scope scope{snapshot,3,4,100};
parties::Current current(uintptr_t pointer=200,uint64_t uid=300){
    return {saved_key,{scope,pointer,uid,false},{pointer,uid,100,400,400}};
}
record::Graph actor(uintptr_t self,uintptr_t party){return {
    {{SIZE_MAX,0,{2,0,"actor"},{}},{0,0,{2,0,"linkId"},{{4,1,""}}},
     {0,1,{2,0,"party"},{{4,2,""}}},{0,2,{2,0,"ordinary_integer"},{{4,2,""}}}},
    {{1,self,true},{2,party,false}}};}
int main()try{
    unsigned checks=0;std::string error;
    const auto check=[&](bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);};
    std::map<uintptr_t,std::string> owners{{99,"other-owner"}};
    check(aliases::append(scope,{},owners,error)&&owners.size()==1,"Empty actual Party census changed other owners");
    check(aliases::append(scope,{current()},owners,error)&&owners.at(200)==saved_key,error);
    check(aliases::append(scope,{current()},owners,error),"Same existing canonical owner was rejected");
    auto moved=current();moved.member.shard=500;moved.member.region=600;moved.member.manager_region=0;
    check(aliases::append(scope,{moved},owners,error),"Actual manager transfer renamed a world-scoped Party");
    auto birth=current(201,301);birth.key=identity::birth(snapshot,0);
    check(aliases::append(scope,{birth},owners,error)&&owners.at(201)==birth.key,error);
    const auto reject=[&](auto mutate,const char* cause){
        auto changed=current(800,900);mutate(changed);
        auto before=owners;check(!aliases::append(scope,{changed},owners,error)&&owners==before&&!error.empty(),cause);
    };
    reject([](auto& row){row.binding.scope.snapshot="abcdefab-1234-1234-1234-123456789abc";},"Foreign snapshot accepted");
    reject([](auto& row){row.binding.scope.epoch++;},"Foreign native epoch accepted");
    reject([](auto& row){row.binding.scope.generation++;},"Foreign native generation accepted");
    reject([](auto& row){row.binding.scope.root++;},"Foreign actual Root accepted");
    reject([](auto& row){row.binding.retired=true;},"Retired Party alias accepted");
    reject([](auto& row){row.binding.pointer=0;},"Null native Party accepted");
    reject([](auto& row){row.binding.uid=0;},"Zero actual full UID accepted");
    reject([](auto& row){row.member.pointer++;},"Member pointer and binding mismatch accepted");
    reject([](auto& row){row.member.uid++;},"Member full UID and binding mismatch accepted");
    reject([](auto& row){row.key="party:manager-slot:2";},"Invented manager-index Party identity accepted");
    reject([](auto& row){row.binding.pointer=99;row.member.pointer=99;},"Another canonical external owner was overwritten");
    const auto duplicate=[&](auto mutate){
        auto first=current(800,900);auto second=current(801,901);second.key=identity::birth(snapshot,1);mutate(second,first);
        const auto before=owners;check(!aliases::append(scope,{first,second},owners,error)&&owners==before,
                                      "Duplicate current Party alias accepted or partial owners committed");
    };
    duplicate([](auto& second,const auto& first){second.key=first.key;});
    duplicate([](auto& second,const auto& first){second.binding.pointer=first.binding.pointer;second.member.pointer=first.member.pointer;});
    duplicate([](auto& second,const auto& first){second.binding.uid=first.binding.uid;second.member.uid=first.member.uid;});
    auto invalid=scope;invalid.generation=0;const auto before=owners;
    check(!aliases::append(invalid,{current()},owners,error)&&owners==before,"Uncompleted World accepted");
    std::map<uintptr_t,std::string> other;
    check(aliases::append(scope,{current(1200,1300)},other,error),error);
    const auto resolve=[&](const auto& map,uintptr_t pointer,std::string& key,std::string& cause){
        const auto found=map.find(pointer);if(found==map.end()){cause="original unknown Party external owner";return false;}
        key=found->second;return true;
    };
    complete::Witness host,client;
    const auto host_owner=[&](uintptr_t p,std::string& key,std::string& cause){return resolve(owners,p,key,cause);};
    const auto client_owner=[&](uintptr_t p,std::string& key,std::string& cause){return resolve(other,p,key,cause);};
    for(size_t family=0;family<4;++family){
        check(complete::summarize(actor(10,200),10,host_owner,host,error),error);
        check(complete::summarize(actor(1010,1200),1010,client_owner,client,error)&&host==client,
              "Actual copied Party canonical key did not normalize the full Actor reference across peers");
        auto changed=actor(1010,1200);changed.records[3].tokens[0].bits=3;
        check(complete::summarize(changed,1010,client_owner,client,error)&&host.sha!=client.sha,
              "Ordinary native integer matching Party link ID was rewritten");
        check(!complete::summarize(actor(1010,2000),1010,client_owner,client,error)&&error=="original unknown Party external owner",
              "Unknown original Party birth acquired a fabricated external owner or lost its cause");
    }
    std::cout<<checks<<" current Party Actor external-owner, full Record and no-fallback comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
