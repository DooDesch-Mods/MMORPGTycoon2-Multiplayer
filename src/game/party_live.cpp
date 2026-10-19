#include "party_live.h"
#include "duplicate_records.h"
#include <algorithm>

namespace mt2mp::game::party_live {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
const Member* member(const Raw& raw,uintptr_t pointer,uint64_t uid){
    const auto found=std::find_if(raw.members.begin(),raw.members.end(),[&](const auto& row){return row.pointer==pointer&&row.uid==uid;});
    return found==raw.members.end()?nullptr:&*found;
}
bool original_key(const std::string& operation,uint32_t ordinal,const std::string& key,std::string& error){
    return (party_identity::uuid(operation)&&key==party_identity::birth(operation,ordinal)&&party_identity::valid_key(key))||
        fail(error,"Party birth lacks its actual action/frame UUID and stable original birth ordinal");
}
bool matches(const Record& a,const Record& b){
    return a.sha==b.sha&&a.toons==b.toons&&a.shard==b.shard&&a.region==b.region&&a.manager_region==b.manager_region&&a.managers==b.managers;
}
bool removed(const Raw& before,const Raw& after,const Member& retired,std::string& error){
    if(before.scope!=after.scope||before.pools.size()!=after.pools.size())return fail(error,"Party native retirement changed complete World/manager census");
    auto expected_members=before.members;
    std::erase_if(expected_members,[&](const auto& row){return row.pointer==retired.pointer&&row.uid==retired.uid;});
    if(expected_members!=after.members)return fail(error,"Party native retirement changed an unrelated current full-UID member");
    for(size_t i=0;i<before.pools.size();++i){auto expected=before.pools[i];
        const auto removed_count=std::erase(expected.parties.members,retired.pointer);
        expected.parties.count-=int32_t(removed_count);
        std::erase_if(expected.members,[&](const auto& row){return row.pointer==retired.pointer&&row.uid==retired.uid;});
        if(expected!=after.pools[i])return fail(error,"Party original ordered removal changed unrelated native manager fields/order/queues");
    }
    return true;
}
}
bool captured(const subscriber_lifecycle::Graph& graph,uintptr_t root,const References& refs,Record& value,std::string& error){
    if(!refs.identify)return fail(error,"Party original Record canonical owner reader is absent");
    value.records=graph.records;value.local_ids.clear();value.external.clear();value.root_link=0;value.sha.clear();
    for(const auto& link:graph.private_links){
        if(link.object==root&&!link.has_inline_definition)return fail(error,"Party original root lacks its native inline link definition");
        if(link.has_inline_definition)value.local_ids.push_back(link.saved_link_id);
        else{std::string key;if(!refs.identify(link.object,key,error))return false;value.external.push_back({link.saved_link_id,std::move(key)});}
        if(link.object==root){if(value.root_link)return fail(error,"Party original root repeats in SaveContext");value.root_link=link.saved_link_id;}
    }
    std::string portable;if(!record_detail::summarize(value,portable,error))return false;
    duplicate_records::Witness original;if(!duplicate_records::summarize(graph,root,refs.identify,original,error))return false;
    if(portable!=original.sha)return fail(error,"Party complete native and portable original Record manifests differ");
    value.sha=std::move(portable);return true;
}
bool Adapter::fresh(const action_session::Context& context,const action_session::Binding& binding,Raw& raw,std::string& error){
    if(!native_.current(context,binding,raw,error))return false;
    if(!party_identity::valid(raw.scope)||raw.scope.snapshot!=raw.world.uuid||raw.scope.epoch!=raw.world.world_epoch||raw.scope.generation!=raw.world.generation||
       raw.scope.epoch!=binding.native_epoch||raw.scope.generation!=binding.native_generation||raw.scope.root!=uintptr_t(raw.world.map)+0x1c0)
        return fail(error,"Party native census has a foreign actual Frame/Session lifetime: session_snapshot="+context.snapshot_uuid+
            " native_snapshot="+raw.world.uuid+" scope_snapshot="+raw.scope.snapshot+
            " native_epoch="+std::to_string(raw.world.world_epoch)+" scope_epoch="+std::to_string(raw.scope.epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " native_generation="+std::to_string(raw.world.generation)+" scope_generation="+std::to_string(raw.scope.generation)+" binding_generation="+std::to_string(binding.native_generation)+
            " native_root="+std::to_string(uintptr_t(raw.world.map)+0x1c0)+" scope_root="+std::to_string(raw.scope.root));
    if(locked_scope_==raw.scope&&!locked_cause_.empty()){error=locked_cause_;return false;}
    return true;
}
bool Adapter::unknown(const party_identity::Scope& scope,Receipt& receipt,std::string& error){
    receipt.unknown=true;receipt.cause=error.empty()?"Party native outcome is unknown; original operation must not replay":error;
    if(locked_cause_.empty()||locked_scope_!=scope){locked_scope_=scope;locked_cause_=receipt.cause;}
    error=locked_cause_;return false;
}
bool Adapter::raw_current(const action_session::Context& context,const action_session::Binding& binding,Raw& out,std::string& error){
    return fresh(context,binding,out,error);
}
bool Adapter::bind_host_birth(const action_session::Context& context,const action_session::Binding& binding,const Raw& before,
    const std::string& operation,uint32_t ordinal,const Member& original,Receipt& receipt,std::string& error){
    receipt={};Raw raw;const auto key=party_identity::birth(operation,ordinal);
    if(context.role!=action_session::Role::Host)return fail(error,"Party original birth adoption requires the actual Host");
    if(!original_key(operation,ordinal,key,error)||!fresh(context,binding,raw,error))return false;
    if(before.scope!=raw.scope||member(before,original.pointer,original.uid))return fail(error,"Party Host birth was already present before the actual native frame operation");
    const auto actual=member(raw,original.pointer,original.uid);if(!actual||*actual!=original)return fail(error,"Party Host birth is not in the complete actual current native census");
    std::vector<std::pair<std::string,party_identity::Entry>> prior;
    if(!party_identity::registry.enumerate(raw.scope,prior,error))return false;
    for(const auto& [old,row]:prior)if(old==key||row.uid==original.uid||(!row.retired&&row.pointer==original.pointer))
        return fail(error,"Party Host original birth already has a native canonical lifetime");
    if(!party_identity::registry.append(key,{raw.scope,original.pointer,original.uid,false},error))return unknown(raw.scope,receipt,error);
    receipt.member=original;receipt.returned=true;receipt.confirmed=true;return true;
}
bool Adapter::current_record_held(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,
    const References& refs,Record& value,std::string& error){
    value={};Raw raw;party_identity::Entry current;
    if(!fresh(context,binding,raw,error)||!party_identity::registry.lookup(raw.scope,key,current,error))return false;
    const auto found=member(raw,current.pointer,current.uid);if(!found)return fail(error,"Party current canonical lifetime is absent from complete native census");
    value.key=key;return native_.capture(*found,refs,value,error)&&validate(value,error);
}
bool Adapter::current_records_held(const action_session::Context& context,const action_session::Binding& binding,
    const References& refs,std::vector<Record>& out,std::string& error){
    out.clear();Raw before;if(!fresh(context,binding,before,error))return false;
    std::vector<Record> ready;
    for(const auto& row:before.members){Record value;
        if(!party_identity::registry.identify(before.scope,row.pointer,row.uid,value.key,error)||
           !native_.capture(row,refs,value,error)||!validate(value,error))return false;
        ready.push_back(std::move(value));
    }
    Raw after;if(!fresh(context,binding,after,error))return false;
    if(after.scope!=before.scope||after.members!=before.members||after.pools!=before.pools)
        return fail(error,"Party complete original Record batch changed actual current native membership");
    std::sort(ready.begin(),ready.end(),[](const auto& a,const auto& b){return a.key<b.key;});
    out=std::move(ready);return true;
}
bool Adapter::client_birth_held(const action_session::Context& context,const action_session::Binding& binding,const std::string& operation,uint32_t ordinal,
    const Record& target,const References& refs,Receipt& receipt,std::string& error){
    receipt={};Raw before;
    if(context.role!=action_session::Role::Client)return fail(error,"Party original birth replay requires the actual Client");
    if(!original_key(operation,ordinal,target.key,error)||!validate(target,error)||!fresh(context,binding,before,error))return false;
    std::vector<std::pair<std::string,party_identity::Entry>> entries;
    if(!party_identity::registry.enumerate(before.scope,entries,error))return false;
    for(const auto& [key,row]:entries)if(key==target.key){
        if(row.retired)return fail(error,"Party original birth key belongs to a retired lifetime");
        Record existing;if(!current_record(context,binding,key,refs,existing,error)||!matches(existing,target))
            return fail(error,error.empty()?"Party repeated birth differs from confirmed current complete Record":error.c_str());
        const auto prior=member(before,row.pointer,row.uid);if(!prior)return fail(error,"Party repeated birth lacks its actual current canonical member");
        receipt.member=*prior;receipt.existing=true;receipt.confirmed=true;return true;
    }
    Member born;
    if(!native_.birth(target,refs,born,receipt,error))return receipt.attempted?unknown(before.scope,receipt,error):false;
    receipt.returned=true;Raw after;
    if(!fresh(context,binding,after,error)||after.scope!=before.scope||!member(after,born.pointer,born.uid)||
       (member(before,born.pointer,born.uid)&&!receipt.reserved))return unknown(before.scope,receipt,error);
    Record actual;actual.key=target.key;
    if(!native_.capture(born,refs,actual,error)||!validate(actual,error)||!matches(actual,target)){
        if(error.empty())error="Party original birth complete Record/member readback differs from Host";
        return unknown(before.scope,receipt,error);
    }
    if(!party_identity::registry.append(target.key,{before.scope,born.pointer,born.uid,false},error))return unknown(before.scope,receipt,error);
    receipt.member=born;receipt.confirmed=true;return true;
}
bool Adapter::client_retire_held(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,Receipt& receipt,std::string& error){
    receipt={};Raw before;party_identity::Entry entry;
    if(context.role!=action_session::Role::Client)return fail(error,"Party original retirement replay requires the actual Client");
    if(!fresh(context,binding,before,error)||!party_identity::registry.lookup(before.scope,key,entry,error))return false;
    const auto found=member(before,entry.pointer,entry.uid);if(!found)return fail(error,"Party retirement has no actual current member");
    if(!native_.remove(before,*found,receipt,error))return receipt.attempted?unknown(before.scope,receipt,error):false;
    Raw after;uintptr_t remaining=0;
    if(!fresh(context,binding,after,error)||!removed(before,after,*found,error)||member(after,entry.pointer,entry.uid)||
       !native_.find(entry.uid,remaining,error)||remaining)return unknown(before.scope,receipt,error);
    if(!party_identity::registry.retire(key,entry,error))return unknown(before.scope,receipt,error);
    receipt.returned=true;receipt.confirmed=true;return true;
}
bool Adapter::client_adopt_held(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,
    const Record& target,const References& refs,Receipt& receipt,std::string& error){
    receipt={};Raw before;party_identity::Entry entry;
    if(context.role!=action_session::Role::Client||target.key!=key)return fail(error,"Party current-state adoption requires the actual Client and exact canonical target");
    if(!validate(target,error)||!fresh(context,binding,before,error)||!party_identity::registry.lookup(before.scope,key,entry,error))return false;
    const auto found=member(before,entry.pointer,entry.uid);if(!found)return fail(error,"Party state adoption lacks its actual current native lifetime");
    const Member prior=*found;
    if(!native_.prepared(key)){
        Record previous;previous.key=key;
        if(!native_.capture(prior,refs,previous,error)||!validate(previous,error))return false;
        if(matches(previous,target)){receipt.member=prior;receipt.existing=true;receipt.confirmed=true;return true;}
    }
    if(!native_.adopt(prior,target,refs,receipt,error))return receipt.attempted?unknown(before.scope,receipt,error):false;
    Raw after;
    if(!fresh(context,binding,after,error)||after.scope!=before.scope||after.members.size()!=before.members.size())return unknown(before.scope,receipt,error);
    const auto current=member(after,entry.pointer,entry.uid);
    if(!current){error="Party current-state adoption changed the existing native Party pointer/full UID";return unknown(before.scope,receipt,error);}
    for(const auto& row:before.members)if(row.pointer!=entry.pointer&&!member(after,row.pointer,row.uid)){
        error="Party current-state adoption changed an unrelated native Party lifetime";return unknown(before.scope,receipt,error);}
    Record actual;actual.key=key;
    if(!native_.capture(*current,refs,actual,error)||!validate(actual,error)||!matches(actual,target)){
        if(error.empty())error="Party current-state complete original Record/member/manager readback differs from Host";
        return unknown(before.scope,receipt,error);
    }
    party_identity::Entry confirmed;
    if(!party_identity::registry.lookup(after.scope,key,confirmed,error)||confirmed!=entry)return unknown(before.scope,receipt,error);
    receipt.member=*current;receipt.returned=true;receipt.confirmed=true;return true;
}
bool Adapter::host_retired(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,
    const party_identity::Entry& before,Receipt& receipt,std::string& error){
    receipt={};Raw after;party_identity::Entry current;uintptr_t pointer=0;
    if(context.role!=action_session::Role::Host)return fail(error,"Party original retirement adoption requires the actual Host");
    if(!fresh(context,binding,after,error)||
       !party_identity::registry.lookup(after.scope,key,current,error))return false;
    if(before.scope!=after.scope||current!=before||member(after,before.pointer,before.uid)||!native_.find(before.uid,pointer,error)||pointer)
        return fail(error,error.empty()?"Party Host original retirement lacks complete actual absence":error.c_str());
    if(!party_identity::registry.retire(key,before,error))return false;
    receipt.confirmed=true;receipt.returned=true;return true;
}
bool Adapter::current_record(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,
    const References& refs,Record& value,std::string& error){
    const bool ok=native_.batch(context,binding,[&](std::string& cause){return current_record_held(context,binding,key,refs,value,cause);},error);
    if(!ok)value={};return ok;
}
bool Adapter::current_records(const action_session::Context& context,const action_session::Binding& binding,
    const References& refs,std::vector<Record>& value,std::string& error){
    const bool ok=native_.batch(context,binding,[&](std::string& cause){return current_records_held(context,binding,refs,value,cause);},error);
    if(!ok)value.clear();return ok;
}
bool Adapter::client_birth(const action_session::Context& context,const action_session::Binding& binding,const std::string& operation,uint32_t ordinal,
    const Record& target,const References& refs,Receipt& receipt,std::string& error){
    receipt={};const bool ok=native_.batch(context,binding,[&](std::string& cause){return client_birth_held(context,binding,operation,ordinal,target,refs,receipt,cause);},error);
    if(!ok&&(receipt.attempted||receipt.confirmed)){receipt.unknown=true;receipt.confirmed=false;receipt.cause=error;}return ok;
}
bool Adapter::client_retire(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,Receipt& receipt,std::string& error){
    receipt={};const bool ok=native_.batch(context,binding,[&](std::string& cause){return client_retire_held(context,binding,key,receipt,cause);},error);
    if(!ok&&(receipt.attempted||receipt.confirmed)){receipt.unknown=true;receipt.confirmed=false;receipt.cause=error;}return ok;
}
bool Adapter::client_adopt(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,
    const Record& target,const References& refs,Receipt& receipt,std::string& error){
    receipt={};const bool ok=native_.batch(context,binding,[&](std::string& cause){return client_adopt_held(context,binding,key,target,refs,receipt,cause);},error);
    if(!ok&&(receipt.attempted||receipt.confirmed)){receipt.unknown=true;receipt.confirmed=false;receipt.cause=error;}return ok;
}
bool Adapter::reserve_birth(const action_session::Context& context,const action_session::Binding& binding,const std::string& operation,uint32_t ordinal,
    const Record& target,Reservation& reservation,Receipt& receipt,std::string& error){
    reservation={};receipt={};Raw before;
    if(context.role!=action_session::Role::Client)return fail(error,"Party factory reservation requires the actual Client");
    if(!original_key(operation,ordinal,target.key,error)||!validate(target,error)||!fresh(context,binding,before,error))return false;
    std::vector<std::pair<std::string,party_identity::Entry>> aliases;
    if(!party_identity::registry.enumerate(before.scope,aliases,error))return false;
    if(std::any_of(aliases.begin(),aliases.end(),[&](const auto& row){return row.first==target.key;}))
        return fail(error,"Party factory reservation targets an existing or retired canonical lifetime");
    Member actual;
    if(!native_.reserve(target.key,actual,receipt,error))return receipt.attempted?unknown(before.scope,receipt,error):false;
    if(!actual.pointer||!actual.uid||member(before,actual.pointer,actual.uid)){
        error="Party original factory reservation lacks a distinct actual native lifetime";return unknown(before.scope,receipt,error);}
    uintptr_t registered=0;
    if(!native_.find(actual.uid,registered,error)||registered!=actual.pointer)return unknown(before.scope,receipt,error);
    reservation={before.scope,target.key,actual};receipt.member=actual;receipt.returned=true;
    return true;
}
bool Adapter::resolve_reserved(const action_session::Context& context,const action_session::Binding& binding,const std::string& key,
    uintptr_t& pointer,std::string& error){
    pointer=0;Raw current;Member actual;
    if(context.role!=action_session::Role::Client||!party_identity::valid_key(key))return fail(error,"Party pending lookup has a foreign role or canonical key");
    if(!fresh(context,binding,current,error)||!native_.reserved(key,actual,error))return false;
    uintptr_t registered=0;
    if(!actual.pointer||!actual.uid||!native_.find(actual.uid,registered,error)||registered!=actual.pointer)
        return fail(error,error.empty()?"Party pending factory lookup lost its actual native full UID":error.c_str());
    pointer=actual.pointer;return true;
}
}
