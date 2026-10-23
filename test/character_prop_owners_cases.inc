namespace prop_owners_fixture {
using json=nlohmann::json;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
#include "character_owner_add.inc"
struct Fixture {
    json rows=json::array();std::map<uintptr_t,std::string> owners;
    int identities=0,refunds=0,lookups=0;bool collector_ok=true,refund_ok=true;
    uint64_t missing_uid=0,throw_uid=0;uintptr_t alias_pointer=0;
};
Fixture* fixture=nullptr;
namespace remove_diagnostics {
bool identity_bindings(json& out,std::string& error){
    ++fixture->identities;if(!fixture->collector_ok)return fail(error,"retained current identity collector failure");
    out={{"bindings",fixture->rows}};return true;
}
bool bindings(json& out,std::string& error){
    if(!identity_bindings(out,error))return false;++fixture->refunds;
    if(!fixture->refund_ok)return fail(error,"unneeded native refund read failure");
    for(auto& row:out["bindings"])row["native_refund"]={{"confirmed",false},{"cause","Scenery has no Building refund"}};
    return true;
}
}
uint32_t lookup(int,uint64_t uid,uintptr_t& pointer){
    ++fixture->lookups;pointer=uid&&uid!=fixture->missing_uid?(fixture->alias_pointer?fixture->alias_pointer:uintptr_t(uid+0x10000)):0;
    return uid==fixture->throw_uid?5u:0u;
}
bool collect(Fixture& current,bool identity,std::string& error){
    fixture=&current;const std::string snapshot="current-snapshot";auto& owners=current.owners;
    struct Out {struct Api {int find=0;}api;}out;
    if(identity){
#include "character_prop_owners.inc"
    }else{
#include "character_prop_owners_refund.inc"
    }
    return true;
}
json row(uint64_t uid){return {{"local_uid",uid},{"record_key","prop:"+std::to_string(uid)},
    {"revoked",false},{"unknown",false},{"snapshot_id","current-snapshot"}};}
void run(){
    Fixture before;for(uint64_t uid=1;uid<=1528;++uid)before.rows.push_back(row(uid));
    Fixture after=before;std::string error;
    need(collect(before,false,error),"original complete refund bindings owner traversal failed");
    need(collect(after,true,error)&&after.owners==before.owners,"identity traversal changed complete canonical prop names");
    need(before.refunds==1&&after.refunds==0&&before.identities==1&&after.identities==1&&before.lookups==1528&&after.lookups==1528,
         "identity traversal omitted a live full UID lookup or requested unused refunds");
    const auto rejected=[&](auto corrupt){Fixture original,candidate;original.rows=json::array({row(1),row(2)});corrupt(original);candidate=original;
        need(!collect(original,false,error),"original owner traversal accepted invalid identity");const auto cause=error;
        need(!collect(candidate,true,error)&&error==cause&&!candidate.refunds,"identity traversal changed rejection cause or admitted invalid identity");
    };
    rejected([](Fixture& value){value.collector_ok=false;});
    rejected([](Fixture& value){value.rows[0]["unknown"]=true;});
    rejected([](Fixture& value){value.rows[0]["snapshot_id"]="foreign-snapshot";});
    rejected([](Fixture& value){value.missing_uid=1;});
    rejected([](Fixture& value){value.throw_uid=1;});
    rejected([](Fixture& value){value.rows[0]["record_key"]="";});
    rejected([](Fixture& value){value.alias_pointer=0x10001;});
    rejected([](Fixture& value){value.owners.emplace(0x10001,"foreign-owner");});
    Fixture revoked;revoked.rows=json::array({row(1),row(2)});revoked.rows[0]["revoked"]=true;revoked.rows[0]["unknown"]=true;
    need(collect(revoked,true,error)&&revoked.lookups==1&&revoked.owners.size()==1,"revoked identity no longer skips its unavailable native UID");
    Fixture unused;unused.rows=json::array({row(1)});unused.refund_ok=false;
    need(collect(unused,true,error)&&unused.lookups==1&&!unused.refunds,"canonical owner naming still depends on unused refund work");
    Fixture empty;need(collect(empty,true,error)&&empty.identities==1&&!empty.lookups&&!empty.refunds,"empty props skipped current collector admission");
    std::cout<<"character prop owners: identical 1528 keys/full UID lookups; unused refund passes 1 -> 0; identity rejection causes preserved\n";
}
}
