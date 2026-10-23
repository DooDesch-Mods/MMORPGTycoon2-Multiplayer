#include "character_catalogue_record.h"
namespace catalogue_manifest_fixture {
namespace batch=mt2mp::game::character_catalogue_batch;
namespace canonical=mt2mp::game::character_catalogue_record;
namespace original=mt2mp::game::duplicate_records;
namespace definitions=mt2mp::game::character_definition_identity;
using Mode=batch::Mode;
using Result=batch::Result;
using Graph=subscriber_lifecycle::Graph;
using json=nlohmann::json;
Graph graph(uintptr_t root,int shape){
    Graph value;value.records={{SIZE_MAX,0,{2,0,"character"},{{4,7,{}}}}};
    if(shape==1||shape==3){
        value.records.push_back({0,0,{2,0,"linkId"},{{4,1,{}}}});
        value.private_links.push_back({1,root+1,true});
    }
    if(shape==2||shape==3)value.private_links.push_back({int32_t(value.private_links.size()+1),root,false});
    if(shape==4)value.private_links.push_back({1,5000,false});
    return value;
}
struct Sample {definitions::Census definitions;std::map<uintptr_t,std::string> owners;int users=0,world=0;};
Sample initial(){
    Sample value;
    for(int32_t i=0;i<2;++i){definitions::Definition row;row.kind=1;row.id=i;row.key="character:1:"+std::to_string(i);
        row.object=1000+uintptr_t(i);row.uid=100+uint64_t(i);value.definitions.definitions.push_back(row);}
    value.definitions.arrays[1].count=2;value.definitions.arrays[1].members={1000,1001};return value;
}
bool stable(const Sample& a,const Sample& b){
    return batch::same(a.definitions,b.definitions)&&a.owners==b.owners&&a.users==b.users&&a.world==b.world;
}
struct Fixture {
    Sample native=initial();std::array<int,2> preparations{},records{};int map_calls=0,shape=0,external_at=0;
    int fail_prepare=0,fail_record=0,drift_endpoint=0,drift_kind=0,change_second_pass=0;
    Mode failure_mode=Mode::LinkFree;bool unknown=false,wrong_fast_bytes=false;
    std::vector<std::string> events;
    static size_t index(Mode mode){return mode==Mode::LinkFree?0:1;}
    bool prepare(Mode mode,Sample& out,std::string& error){
        const auto call=++preparations[index(mode)];events.push_back(mode==Mode::LinkFree?"fast prepare":"eager prepare");
        if(mode==failure_mode&&call==fail_prepare){error="exact preparation failure";return false;}
        out=native;
        if(mode==Mode::Eager){++map_calls;events.push_back("full external map");if(!unknown)out.owners.emplace(5000,"scenery:canonical");}
        if(mode==failure_mode&&call==drift_endpoint)switch(drift_kind){
            case 0:++out.world;break;
            case 1:++out.users;break;
            case 2:++out.definitions.definitions[0].uid;break;
            case 3:++out.definitions.arrays[1].data;break;
            case 4:++out.definitions.manager;break;
            case 5:out.owners.emplace(9000,"foreign owner");break;
        }
        return true;
    }
    Result record(Mode mode,const Sample& sample,const definitions::Definition& definition,std::string& out,std::string& error){
        const auto call=++records[index(mode)];events.push_back(mode==Mode::LinkFree?"fast native record":"eager native record");
        if(mode==failure_mode&&call==fail_record){error="exact native capture failure";return Result::Failed;}
        auto value=graph(definition.object,external_at?(mode==Mode::Eager||call==external_at?4:0):shape);
        if(mode==Mode::LinkFree&&wrong_fast_bytes)value.records[0].tokens[0].bits=123;
        if(mode==failure_mode&&change_second_pass&&call>2)++value.records[0].tokens[0].bits;
        return canonical::summarize(value,definition.object,sample.owners,mode,out,error);
    }
    bool capture(json& out,std::string& error){
        auto preparing=[&](Mode mode,Sample& sample,std::string& cause){return prepare(mode,sample,cause);};
        auto recording=[&](Mode mode,const Sample& sample,const auto& definition,auto& sha,auto& cause){return record(mode,sample,definition,sha,cause);};
        return batch::capture_manifest<Sample>(preparing,recording,stable,out,error);
    }
    bool eager(json& out,std::string& error){
        auto preparing=[&](Sample& sample,std::string& cause){return prepare(Mode::Eager,sample,cause);};
        auto recording=[&](const Sample& sample,const auto& definition,auto& sha,auto& cause){return record(Mode::Eager,sample,definition,sha,cause)==Result::Complete;};
        return batch::capture<Sample>(preparing,recording,stable,out,error);
    }
};
void run(){
    std::string error,hash;const std::map<uintptr_t,std::string> owners{{5000,"scenery:canonical"}};
    for(int shape=0;shape<=3;++shape){
        const auto source=graph(1000,shape);original::Witness expected;int owner_calls=0;
        auto unused=[&](uintptr_t,std::string&,std::string& cause){++owner_calls;cause="unexpected external resolver";return false;};
        need(original::summarize(source,1000,unused,expected,error)&&owner_calls==0,"baseline internal manifest requested external names");
        need(canonical::summarize(source,1000,{},Mode::LinkFree,hash,error)==Result::Complete&&hash==expected.sha,
             "link-free canonical bytes differ from original root/inline rules");
        Fixture fast,baseline;fast.shape=baseline.shape=shape;json actual,prior;
        need(fast.capture(actual,error)&&baseline.eager(prior,error)&&actual==prior,"link-free complete catalogue differs from eager baseline");
        need(fast.map_calls==0&&fast.preparations==std::array<int,2>{3,0}&&fast.records==std::array<int,2>{4,0},
             "link-free catalogue did not retain two native passes and three fresh endpoints without external maps");
    }
    auto external=graph(1000,4);
    need(canonical::summarize(external,1000,owners,Mode::LinkFree,hash,error)==Result::NeedsExternalOwners&&hash.empty(),
         "actual external reference did not request eager capture even when a name is available");
    need(canonical::summarize(external,1000,{},Mode::Eager,hash,error)==Result::Failed&&
         error=="Character complete original definition Record contains an unresolved external reference",
         "eager unknown reference lost its original failure");
    auto bad=graph(1000,1);bad.private_links[0].has_inline_definition=false;
    need(canonical::summarize(bad,1000,{},Mode::LinkFree,hash,error)==Result::Failed&&error=="Duplicate original inline link manifest differs",
         "malformed inline manifest became fallback or success");
    bad=graph(1000,2);bad.private_links[0].saved_link_id=2;
    need(canonical::summarize(bad,1000,{},Mode::LinkFree,hash,error)==Result::Failed&&error=="Duplicate original SaveContext IDs are not complete from one",
         "root reference bypassed complete SaveContext IDs");
    bad=external;bad.private_links.push_back({3,1000,false});
    need(canonical::summarize(bad,1000,{},Mode::LinkFree,hash,error)==Result::Failed&&error=="Duplicate original SaveContext IDs are not complete from one",
         "first external reference concealed an invalid later manifest ID");
    for(int demand=1;demand<=4;++demand){
        Fixture fast,baseline;fast.external_at=demand;fast.wrong_fast_bytes=true;baseline.shape=4;json actual,prior;
        need(fast.capture(actual,error)&&baseline.eager(prior,error)&&actual==prior,"external fallback mixed preflight bytes into eager canonical result");
        need(fast.records[0]==demand&&fast.records[1]==4&&fast.preparations[1]==3&&fast.map_calls==3,
             "external fallback did not restart both complete eager passes and all three full maps");
        const auto start=std::find(fast.events.begin(),fast.events.end(),"eager prepare");
        need(start!=fast.events.end()&&std::distance(start,fast.events.end())>=3&&start[1]=="full external map"&&start[2]=="eager native record",
             "fallback resolved references after serialization instead of recapturing from a fresh pre-state");
    }
    Fixture unknown;unknown.external_at=1;unknown.unknown=true;json out;
    need(!unknown.capture(out,error)&&out.is_null()&&error=="Character complete original definition Record contains an unresolved external reference"&&unknown.map_calls==1,
         "fallback hid an unresolved external owner or published partial output");
    for(Mode mode:{Mode::LinkFree,Mode::Eager}){
        for(int point=1;point<=3;++point){Fixture failed;failed.failure_mode=mode;failed.fail_prepare=point;if(mode==Mode::Eager)failed.external_at=1;
            need(!failed.capture(out,error)&&out.is_null()&&error=="exact preparation failure"&&failed.preparations[Fixture::index(mode)]==point,
                 "manifest preparation failure was retried or hidden");}
        for(int point=1;point<=4;++point){Fixture failed;failed.failure_mode=mode;failed.fail_record=point;if(mode==Mode::Eager)failed.external_at=1;
            need(!failed.capture(out,error)&&out.is_null()&&error=="exact native capture failure"&&failed.records[Fixture::index(mode)]==point,
                 "manifest native failure was retried or hidden");}
        for(int endpoint=2;endpoint<=3;++endpoint)for(int kind=0;kind<6;++kind){Fixture drift;drift.failure_mode=mode;
            drift.drift_endpoint=endpoint;drift.drift_kind=kind;if(mode==Mode::Eager)drift.external_at=1;
            need(!drift.capture(out,error)&&out.is_null()&&error=="Character catalogue original records/manager/users/reference owners changed during complete capture",
                 "manifest path accepted changed census/users/world/owner endpoint");}
        Fixture changed;changed.failure_mode=mode;changed.change_second_pass=1;if(mode==Mode::Eager)changed.external_at=1;
        need(!changed.capture(out,error)&&out.is_null()&&error=="Character catalogue original records changed during complete double capture",
             "manifest path accepted changed second-pass native bytes");
    }
    std::cout<<"character catalogue manifest: original hashes, link-free endpoints, full eager fallback and failure/drift paths passed\n";
}
}
