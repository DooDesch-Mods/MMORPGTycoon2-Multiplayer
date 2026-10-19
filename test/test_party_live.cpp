#include "party_live.h"
#include <iostream>

namespace live=mt2mp::game::party_live;
namespace identity=mt2mp::game::party_identity;
namespace typed=mt2mp::action_session;
namespace original=mt2mp::subscriber_lifecycle;
namespace {
const std::string snapshot="12345678-1234-1234-1234-123456789abc";
const std::string operation="aaaaaaaa-bbbb-cccc-dddd-123456789abc";
const identity::Scope world_scope{snapshot,3,4,0x1000};
typed::Context context(typed::Role role){typed::Context out;out.role=role;out.session=1;out.host_peer=1;
    out.local_peer=role==typed::Role::Host?1:2;out.peer_mask=3;out.snapshot_uuid=snapshot;return out;}
typed::Binding binding(){typed::Binding out;out.native_epoch=3;out.native_generation=4;return out;}
live::References references(){return {
    [](uintptr_t pointer,std::string& key,std::string& error){if(pointer==700||pointer==1700){key="toon:9";return true;}
        error="original unresolved Party member reference";return false;},
    [](const std::string& key,uintptr_t& pointer,std::string& error){if(key=="toon:9"){pointer=700;return true;}
        error="original missing canonical Party reference";return false;}};}
original::Graph graph(uintptr_t party=200,uintptr_t toon=700){return {
    {{SIZE_MAX,0,{1,0,"mmoParty"},{}},{0,0,{1,0,"linkId"},{{4,1,{}}}},
     {0,1,{1,0,"mode"},{{4,3,{}}}},{0,2,{1,0,"toon"},{{4,2,{}}}},
     {0,3,{1,0,"ordinary_integer"},{{4,2,{}}}},{0,4,{1,0,"native_extra_field"},{{3,0x3f812345,{}}}}},
    {{1,party,true},{2,toon,false}}};}
live::Record record(const std::string& key=identity::birth(operation,0),uintptr_t pointer=200,uintptr_t toon=700){
    live::Record value;value.key=key;value.shard="root_shard";value.region=value.manager_region="region:0";
    value.managers={{"region:0",false,false}};value.toons={"toon:9"};std::string error;
    if(!live::captured(graph(pointer,toon),pointer,references(),value,error))throw std::runtime_error(error);
    return value;
}
struct Native final:live::Port {
    live::Raw raw;std::map<uintptr_t,live::Record> records;
    std::map<std::string,live::Member> pending;
    std::set<std::string> prepared_keys;
    unsigned calls=0,removes=0,adoptions=0,scopes=0,reads=0,captures=0;
    bool reject_prepared_capture=false,wrong_pending_uid=false;
    bool held=false,fail_after_birth=false,fail_before_birth=false,corrupt_birth=false,corrupt_survivor=false,fail_adopt=false,change_adopt_uid=false;
    Native(){raw.scope=world_scope;raw.world.uuid=snapshot;raw.world.world_epoch=3;raw.world.generation=4;raw.world.map=reinterpret_cast<void*>(world_scope.root-0x1c0);mt2mp::game::dungeon_party_census::Pool pool;pool.manager=400;pool.region=500;pool.shard=100;
        pool.parties.object=424;pool.parties.data=600;pool.parties.capacity=100;raw.pools.push_back(pool);}
    void add(live::Member row,live::Record value){raw.members.push_back(row);auto& pool=raw.pools.front();pool.members.push_back(row);
        pool.parties.members.push_back(row.pointer);++pool.parties.count;records[row.pointer]=std::move(value);}
    bool batch(const typed::Context&,const typed::Binding&,const std::function<bool(std::string&)>& body,std::string& error)override{
        if(held)return body(error);++scopes;held=true;const bool ok=body(error);held=false;
        if(!pending.empty()){if(error.empty())error="original factory reservation unfinished at batch exit";return false;}return ok;
    }
    bool current(const typed::Context&,const typed::Binding&,live::Raw& out,std::string&)override{++reads;out=raw;return true;}
    bool capture(const live::Member& row,const live::References& refs,live::Record& value,std::string& error)override{
        ++captures;
        if(reject_prepared_capture&&prepared_keys.contains(value.key)){error="original Init has not completed before Save";return false;}
        const auto key=value.key;value=records.at(row.pointer);value.key=key;
        auto source=graph(row.pointer);source.records=value.records;
        return live::captured(source,row.pointer,refs,value,error);
    }
    bool birth(const live::Record& target,const live::References&,live::Member& born,live::Receipt& receipt,std::string& error)override{
        if(fail_before_birth){error="original pre-effect owner failure";return false;}
        const auto reserved_party=pending.find(target.key);
        receipt.attempted=true;if(reserved_party==pending.end()){++calls;receipt.calls.factory=1;born={200,900,100,500,500};}
        else{receipt.reserved=true;born=reserved_party->second;born.shard=100;born.region=born.manager_region=500;pending.erase(reserved_party);}
        receipt.calls.load=prepared_keys.erase(target.key)?0:1;receipt.calls.context=2;receipt.calls.toon_bind=1;
        auto value=target;if(corrupt_birth)value.records.back().tokens[0].bits++;
        if(std::none_of(raw.members.begin(),raw.members.end(),[&](const auto& row){return row.pointer==born.pointer&&row.uid==born.uid;}))add(born,std::move(value));
        else records[born.pointer]=std::move(value);
        if(fail_after_birth){error="original Party factory/load effect unknown";return false;}
        receipt.returned=true;return true;
    }
    bool remove(const live::Raw&,const live::Member& row,live::Receipt& receipt,std::string&)override{
        ++removes;receipt.attempted=true;receipt.calls.remove=1;
        std::erase_if(raw.members,[&](const auto& value){return value.pointer==row.pointer;});
        for(auto& pool:raw.pools){const auto count=std::erase(pool.parties.members,row.pointer);pool.parties.count-=int32_t(count);
            std::erase_if(pool.members,[&](const auto& value){return value.pointer==row.pointer;});}
        records.erase(row.pointer);if(corrupt_survivor)raw.pools.front().parties.capacity++;
        receipt.returned=true;return true;
    }
    bool adopt(const live::Member& row,const live::Record& target,const live::References&,live::Receipt& receipt,std::string& error)override{
        ++adoptions;receipt.attempted=true;receipt.calls.load=prepared_keys.erase(target.key)?0:1;records[row.pointer]=target;
        if(change_adopt_uid){raw.members.front().uid++;raw.pools.front().members.front().uid++;}
        if(fail_adopt){error="original existing Party load effect unknown";return false;}
        receipt.returned=true;return true;
    }
    bool find(uint64_t uid,uintptr_t& pointer,std::string&)override{
        pointer=0;for(const auto& row:raw.members)if(row.uid==uid)pointer=row.pointer;
        for(const auto& row:pending)if(row.second.uid==uid)pointer=wrong_pending_uid?row.second.pointer+8:row.second.pointer;return true;
    }
    bool reserve(const std::string& key,live::Member& member,live::Receipt& receipt,std::string& error)override{
        if(!held){error="original factory reservation requires held transaction";return false;}
        const auto found=pending.find(key);if(found!=pending.end()){member=found->second;receipt.existing=true;receipt.returned=true;return true;}
        member={200,900,0,0,0};pending.emplace(key,member);++calls;receipt.attempted=true;receipt.calls.factory=1;receipt.returned=true;return true;
    }
    bool reserved(const std::string& key,live::Member& member,std::string& error)override{
        if(!held||!pending.contains(key)){error="original pending Party factory owner absent";return false;}
        member=pending.at(key);return true;
    }
    bool prepared(const std::string& key)const override{return prepared_keys.contains(key);}
};
void reset(){identity::registry=identity::Registry{};}
}
int main()try{
    unsigned checks=0;std::string error;const auto check=[&](bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);};
    const auto host=context(typed::Role::Host),client=context(typed::Role::Client);const auto bound=binding();const auto refs=references();
    const std::string native_snapshot="113d3d9d-8848-49a7-b0c3-c23124fa283b",shared_snapshot="8aa73b94-2fae-4293-9f6c-705b0aa015b8";
    auto measured_host=host;measured_host.snapshot_uuid=shared_snapshot;const typed::Binding measured_binding{1,1};
    Native measured;measured.raw.scope={native_snapshot,1,1,0x1000};measured.raw.world.uuid=native_snapshot;
    measured.raw.world.world_epoch=1;measured.raw.world.generation=1;measured.raw.world.map=reinterpret_cast<void*>(0xe40);
    live::Adapter measured_adapter(measured);live::Raw observed;
    check(measured_adapter.raw_current(measured_host,measured_binding,observed,error),"Host local load UUID must not equal shared transfer UUID");
    check(observed.scope.snapshot==native_snapshot&&measured.calls==0,"Scope admission rewrote native identity or caused an original effect");
    const auto valid_raw=measured.raw;
    for(unsigned field=0;field<7;++field){measured.raw=valid_raw;
        if(field==0)measured.raw.scope.snapshot=shared_snapshot;
        if(field==1)++measured.raw.scope.epoch;
        if(field==2)++measured.raw.scope.generation;
        if(field==3)++measured.raw.scope.root;
        if(field==4)measured.raw.world.uuid=shared_snapshot;
        if(field==5)++measured.raw.world.world_epoch;
        if(field==6)++measured.raw.world.generation;
        error.clear();check(!measured_adapter.raw_current(measured_host,measured_binding,observed,error),"Foreign native census scope accepted");
        check(error.find("session_snapshot="+shared_snapshot)!=std::string::npos&&error.find("binding_epoch=1")!=std::string::npos&&error.find("binding_generation=1")!=std::string::npos,
              "Native scope diagnostic omitted actual Session or expected Binding values");
    }
    measured.raw=valid_raw;measured.raw.scope.snapshot=shared_snapshot;measured.raw.world.uuid=shared_snapshot;
    auto measured_client=client;measured_client.snapshot_uuid=shared_snapshot;
    check(measured_adapter.raw_current(measured_client,measured_binding,observed,error),"Client shared load UUID no longer admitted");
    auto target=record();check(live::validate(target,error),error);
    const auto rebased=record(target.key,1200,1700);check(target.sha==rebased.sha,"Native Party/Toon pointers leaked into complete portable Record");
    for(size_t index:{size_t(2),size_t(4),size_t(5)}){auto changed=target;changed.records[index].tokens[0].bits++;
        check(!live::validate(changed,error),"Full original Party field was omitted from checksum");}
    auto invalid=target;invalid.local_ids.clear();check(!live::validate(invalid,error),"Missing original inline manifest accepted");
    invalid=target;invalid.external.clear();check(!live::validate(invalid,error),"Missing original Toon link accepted");
    invalid=target;invalid.toons={"toon:other"};check(!live::validate(invalid,error),"Unproven original Party member accepted");
    invalid=target;invalid.managers.clear();check(!live::validate(invalid,error),"Missing actual native manager ownership accepted");
    reset();Native actual;live::Adapter adapter(actual);const auto before=actual.raw;const live::Member born{200,900,100,500,500};
    actual.add(born,target);std::vector<live::Record> values;live::Receipt receipt;
    check(!adapter.current_records(host,bound,refs,values,error)&&values.empty(),"Unknown natural birth obtained a fabricated canonical key");
    check(adapter.bind_host_birth(host,bound,before,operation,0,born,receipt,error)&&receipt.confirmed,error);
    check(actual.calls==0&&actual.removes==0,"Host adoption replayed the already executed original birth");
    const auto scope_count=actual.scopes;check(adapter.current_records(host,bound,refs,values,error)&&values.size()==1&&values[0].sha==target.sha,error);
    check(actual.scopes==scope_count+1,"Complete current Party records opened more than one native batch scope");
    check(!adapter.bind_host_birth(host,bound,before,operation,1,born,receipt,error),"One actual Party lifetime acquired another original birth key");
    check(!adapter.bind_host_birth(host,bound,actual.raw,operation,1,born,receipt,error),"Preexisting native Party adopted as a new birth");
    check(!adapter.bind_host_birth(client,bound,before,operation,1,born,receipt,error)&&!error.empty(),"Client forged Host original birth provenance");
    reset();Native replica;live::Adapter client_adapter(replica);
    check(client_adapter.client_birth(client,bound,operation,0,target,refs,receipt,error)&&receipt.confirmed,error);
    check(replica.calls==1&&receipt.calls.factory==1&&receipt.calls.load==1&&receipt.calls.toon_bind==1,"Original Party mutation did not execute once");
    identity::Entry entry;check(identity::registry.lookup(world_scope,target.key,entry,error)&&entry.pointer==200&&entry.uid==900,error);
    check(client_adapter.client_birth(client,bound,operation,0,target,refs,receipt,error)&&receipt.existing&&replica.calls==1,
          "Confirmed Party birth replayed its original factory");
    check(!client_adapter.client_birth(client,bound,operation,1,target,refs,receipt,error)&&replica.calls==1,"Foreign original birth ordinal executed");
    check(client_adapter.client_retire(client,bound,target.key,receipt,error)&&receipt.confirmed&&replica.removes==1,error);
    check(!identity::registry.lookup(world_scope,target.key,entry,error),"Retired actual Party alias remained live");
    check(!client_adapter.client_retire(client,bound,target.key,receipt,error)&&replica.removes==1,"Confirmed native Party destructor replayed");
    check(!client_adapter.client_birth(client,bound,operation,0,target,refs,receipt,error)&&replica.calls==1,"Original birth revived a retired canonical lifetime");
    for(const bool corrupt:{false,true}){reset();Native failed;failed.fail_after_birth=!corrupt;failed.corrupt_birth=corrupt;live::Adapter guarded(failed);
        check(!guarded.client_birth(client,bound,operation,0,target,refs,receipt,error)&&receipt.unknown&&failed.calls==1,
              "Partial original Party birth became confirmed");
        const auto original_cause=error;
        check(!guarded.client_birth(client,bound,operation,0,target,refs,receipt,error)&&failed.calls==1&&error==original_cause,
              "Unknown original Party birth replayed or lost its first cause");
        check(!identity::registry.lookup(world_scope,target.key,entry,error),"Unknown Party birth published a confirmed alias");
    }
    reset();Native preflight;preflight.fail_before_birth=true;live::Adapter safe(preflight);
    check(!safe.client_birth(client,bound,operation,0,target,refs,receipt,error)&&!receipt.attempted&&preflight.calls==0,
          "Never-executed preflight failure was labeled an executed original birth");
    preflight.fail_before_birth=false;check(safe.client_birth(client,bound,operation,0,target,refs,receipt,error)&&preflight.calls==1,error);
    preflight.corrupt_survivor=true;check(!safe.client_retire(client,bound,target.key,receipt,error)&&receipt.unknown&&preflight.removes==1,
        "Native retirement changed unrelated manager state without becoming Unknown");
    check(!safe.client_retire(client,bound,target.key,receipt,error)&&preflight.removes==1,"Unknown original retirement replayed");
    reset();Native natural;live::Adapter host_adapter(natural);const identity::Entry previous{world_scope,200,900,false};
    check(identity::registry.append(target.key,previous,error),error);
    check(host_adapter.host_retired(host,bound,target.key,previous,receipt,error)&&receipt.confirmed&&natural.removes==0,
          "Already completed Host native retirement was re-executed");
    check(!identity::registry.lookup(world_scope,target.key,entry,error),"Confirmed Host native retirement kept its Party alias");
    reset();Native current;current.add(born,target);live::Adapter state(current);
    check(identity::registry.append(target.key,previous,error),error);
    check(state.client_adopt(client,bound,target.key,target,refs,receipt,error)&&receipt.existing&&current.adoptions==0,
          "Unchanged existing Party state executed an original load");
    auto moved=target;moved.records.back().tokens[0].bits++;
    auto moved_graph=graph();moved_graph.records=moved.records;
    check(live::captured(moved_graph,200,refs,moved,error),error);
    check(state.client_adopt(client,bound,target.key,moved,refs,receipt,error)&&receipt.confirmed&&current.adoptions==1,error);
    check(receipt.calls.factory==0&&receipt.calls.load==1&&current.calls==0&&current.removes==0,
          "Existing Party state allocated/replaced/destroyed its native identity");
    check(identity::registry.lookup(world_scope,target.key,entry,error)&&entry==previous,
          "Existing Party state changed its canonical/native full-UID lifetime");
    check(state.client_adopt(client,bound,target.key,moved,refs,receipt,error)&&receipt.existing&&current.adoptions==1,
          "Confirmed current Party state was replayed");
    check(!state.client_adopt(host,bound,target.key,target,refs,receipt,error)&&current.adoptions==1,
          "Host executed Client state adoption");
    for(const bool changed_uid:{false,true}){reset();Native broken;broken.add(born,target);broken.fail_adopt=!changed_uid;broken.change_adopt_uid=changed_uid;
        live::Adapter pending(broken);check(identity::registry.append(target.key,previous,error),error);
        check(!pending.client_adopt(client,bound,target.key,moved,refs,receipt,error)&&receipt.unknown&&broken.adoptions==1,
              "Partial or identity-replacing existing Party load became confirmed");
        const auto cause=error;
        check(!pending.client_adopt(client,bound,target.key,moved,refs,receipt,error)&&broken.adoptions==1&&cause==error,
              "Unknown existing Party state load replayed or lost its cause");
    }
    reset();Native grouped;live::Adapter transaction(grouped);live::Reservation reservation;
    check(!transaction.reserve_birth(client,bound,operation,0,target,reservation,receipt,error)&&!receipt.attempted&&grouped.calls==0,
          "Factory reservation escaped its held shared transaction");
    check(grouped.batch(client,bound,[&](std::string& cause){
        if(!transaction.reserve_birth(client,bound,operation,0,target,reservation,receipt,cause))return false;
        check(receipt.attempted&&!receipt.confirmed&&receipt.calls.factory==1&&grouped.raw.members.empty(),
              "Factory reservation fabricated complete manager admission");
        check(!identity::registry.lookup(world_scope,target.key,entry,error),"Factory-only reservation published a confirmed canonical alias");
        uintptr_t pointer=0;
        check(transaction.resolve_reserved(client,bound,target.key,pointer,cause)&&pointer==reservation.member.pointer,
              "Pending Party provider did not resolve its actual original factory lifetime");
        check(transaction.reserve_birth(client,bound,operation,0,target,reservation,receipt,cause)&&grouped.calls==1&&receipt.existing,
              "Pending original factory reservation replayed");
        if(!transaction.client_birth(client,bound,operation,0,target,refs,receipt,cause))return false;
        check(receipt.confirmed&&receipt.calls.factory==0&&receipt.calls.load==1&&grouped.calls==1,
              "Pending Party finish replaced its original factory object");
        return true;
    },error),error);
    check(grouped.scopes==1&&grouped.pending.empty(),"Cross-birth transaction opened multiple scopes or left a factory object unfinished");
    check(identity::registry.lookup(world_scope,target.key,entry,error)&&entry.pointer==reservation.member.pointer&&entry.uid==reservation.member.uid,
          "Finished Party did not preserve its reserved real pointer/full UID");
    reset();Native unfinished;live::Adapter incomplete(unfinished);
    check(!unfinished.batch(client,bound,[&](std::string& cause){return incomplete.reserve_birth(client,bound,operation,0,target,reservation,receipt,cause);},error)
          &&unfinished.calls==1&&!unfinished.pending.empty(),"Unfinished actual factory transaction became successful");
    check(!identity::registry.lookup(world_scope,target.key,entry,error),"Unfinished factory transaction published a confirmed alias");
    reset();Native staged;staged.add(born,target);staged.prepared_keys.insert(target.key);staged.reject_prepared_capture=true;live::Adapter finalize(staged);
    check(identity::registry.append(target.key,previous,error),error);
    check(finalize.client_adopt(client,bound,target.key,target,refs,receipt,error)&&receipt.confirmed&&!receipt.existing&&staged.adoptions==1,
          "Prepared existing Party skipped required final native Init because its Record was already equal");
    check(receipt.calls.load==0&&staged.calls==0&&staged.removes==0&&staged.prepared_keys.empty(),
          "Prepared existing Party finalization repeated Load/factory/destructor");
    check(staged.captures==1,"Prepared existing Party was saved before deferred Init completed");
    reset();Native preloaded;live::Adapter pending_birth(preloaded);
    check(preloaded.batch(client,bound,[&](std::string& cause){
        if(!pending_birth.reserve_birth(client,bound,operation,0,target,reservation,receipt,cause))return false;
        preloaded.add(born,target);preloaded.prepared_keys.insert(target.key);
        if(!pending_birth.client_birth(client,bound,operation,0,target,refs,receipt,cause))return false;
        check(receipt.confirmed&&receipt.reserved&&receipt.calls.factory==0&&receipt.calls.load==0,
              "Preloaded Party admission forced a second original factory/Load");return true;
    },error),error);
    check(preloaded.raw.members.size()==1&&preloaded.calls==1,"Preloaded original Party appeared twice in its actual manager");
    for(const bool prepared_failure:{false,true}){
        reset();Native broken;live::Adapter guarded(broken);
        check(!broken.batch(client,bound,[&](std::string& cause){
            if(!guarded.reserve_birth(client,bound,operation,0,target,reservation,receipt,cause))return false;
            if(prepared_failure){broken.add(born,target);broken.prepared_keys.insert(target.key);}
            broken.fail_after_birth=true;
            return guarded.client_birth(client,bound,operation,0,target,refs,receipt,cause);
        },error)&&receipt.unknown&&!receipt.confirmed&&broken.calls==1,
            "Reserved/prepared Party failure became confirmed or repeated factory");
        const auto cause=error;
        check(!guarded.client_birth(client,bound,operation,0,target,refs,receipt,error)&&broken.calls==1&&error==cause,
            "Unknown reserved/prepared Party replayed or lost original failure");
        check(!identity::registry.lookup(world_scope,target.key,entry,error),"Failed reserved/prepared Party published an alias");
    }
    reset();Native lost;live::Adapter lifetime(lost);
    check(!lost.batch(client,bound,[&](std::string& cause){
        if(!lifetime.reserve_birth(client,bound,operation,0,target,reservation,receipt,cause))return false;
        lost.wrong_pending_uid=true;uintptr_t pointer=0;
        check(!lifetime.resolve_reserved(client,bound,target.key,pointer,cause)&&pointer==0,
            "Reserved Party provider exposed a replaced full UID");
        return false;
    },error)&&lost.calls==1,"Lost reserved lifetime escaped unfinished transaction");
    check(!identity::registry.lookup(world_scope,target.key,entry,error),"Lost reserved full UID published an alias");
    reset();Native staged_failure;staged_failure.add(born,target);staged_failure.prepared_keys.insert(target.key);
    staged_failure.reject_prepared_capture=true;staged_failure.fail_adopt=true;live::Adapter failed_finalize(staged_failure);
    check(identity::registry.append(target.key,previous,error),error);
    check(!failed_finalize.client_adopt(client,bound,target.key,target,refs,receipt,error)&&receipt.unknown&&staged_failure.adoptions==1,
          "Prepared existing Party failed Init became confirmed");
    const auto final_cause=error;
    check(!failed_finalize.client_adopt(client,bound,target.key,target,refs,receipt,error)&&error==final_cause&&staged_failure.adoptions==1,
          "Prepared existing Party failed Init replayed");
    check(staged_failure.captures==0,"Failed deferred Init was followed by a complete native Save");
    std::cout<<checks<<" actual Party live adapter full Record, original-call, provenance, batch, retirement and Unknown/no-replay comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
