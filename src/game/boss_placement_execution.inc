namespace boss_placement {
namespace {
struct Operation {
    action::Context context;action::Binding binding;action::Request request;Sample before,after;
    action::Evidence evidence;boss_current::Birth birth;boss_placement_transaction::Receipt native;actors::Receipt adoption;
    bool seed_attempted=false,confirmed=false,retired=false,rejected_no_effect=false;std::string cause;
};
std::map<std::string,Operation> operations,quarantined;
bool code(Runtime&,bool,std::string&);
struct Execution {
    Runtime& r;Operation& op;
    bool prepare(const action::BossPlacementIntent&,std::string& error){
        return (op.birth.scope.manager&&op.birth.key==boss_placement_state::key(op.request.action_uuid)&&code(r,true,error))||
            q::fail(error,error.empty()?"Boss native transaction prepared Birth manager/action differs":error);
    }
    bool purchase(const action::BossPlacementIntent& intent,bool& paid,std::string& error){
        const std::array<uint32_t,3> position{intent.pose[4],intent.pose[5],intent.pose[6]};
        const auto result=boss_placement_calls::purchase(r.image,position,paid);
        return !result||q::fail(error,"Boss original Purchase SEH="+std::to_string(result));
    }
    bool region(const action::BossPlacementIntent& intent,uintptr_t& region,std::string& error){
        const auto result=boss_placement_calls::region(r.image,uintptr_t(op.before.world.origin.map),intent.point,region);
        return (!result||q::fail(error,"Boss original transaction RegionAt SEH="+std::to_string(result)))&&
            (region==op.birth.scope.region||q::fail(error,"Boss original transaction release point chose a different prepared Root Region"));
    }
    bool add(uintptr_t region,const action::BossPlacementIntent& intent,uintptr_t& returned,std::string& error){
        uintptr_t manager=0;
        if(!q::at(r.memory,region,0x990,manager,error)||manager!=op.birth.scope.manager)return q::fail(error,error.empty()?"Boss original transaction BossManager changed after Purchase":error);
        std::array<unsigned char,0x6c> pose;if(!boss_placement_reader::transform(intent,pose,error))return false;
        const auto result=boss_placement_calls::add(r.image,manager,intent.type,pose,returned);
        return !result||q::fail(error,"Boss original AddBoss SEH="+std::to_string(result));
    }
    bool confirm(uintptr_t returned,std::string& error){return boss_current::confirm_birth(op.birth,returned,error);}
};
bool context(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,action::Context& out,std::string& error){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||r->thread!=GetCurrentThreadId()||request.action!=action::Action::PlaceBoss||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||!action::valid_boss_placement(request.boss_placement)||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,error)||!action::selected_session().current(out,error)||
       out.snapshot_uuid!=request.snapshot_uuid||!binding.native_epoch||!binding.native_generation)
        return q::fail(error,error.empty()?"Boss current pending Controller, role or bound action differs":error);
    return true;
}
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    action::Context current;if(!context(binding,request,authority,current,error))return action::Outcome::RejectedNoEffect;
    auto* r=published.load(std::memory_order_acquire);
    if(operations.contains(request.action_uuid)||quarantined.contains(request.action_uuid)){error="Boss action was already attempted; inspect retained receipt, no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_boss_placement_effect(request,*authority)){error="Boss complete Host evidence is invalid";return action::Outcome::RejectedNoEffect;}
    Operation value;value.context=current;value.binding=binding;value.request=request;std::string character;auto actual_intent=request.boss_placement;
    const auto born=boss_placement_state::key(request.action_uuid);
    if(!source_intent(*r,current,binding,actual_intent,error)||actual_intent!=request.boss_placement||
       !sample(*r,current,binding,born,value.before,error)||value.before.value.created){
        if(error.empty())error="Boss original selected Character or complete current source changed";return action::Outcome::RejectedNoEffect;}
    if(authority&&(value.before.money!=authority->before||value.before.value.untouched_sha!=authority->boss_placement_before->untouched_sha||
       value.before.value.release!=authority->boss_placement_before->release)){error="Boss Client complete current actors/Ledger/release differ from Host";return action::Outcome::RejectedNoEffect;}
    const boss_current::Pool* pool=nullptr;
    if(!region_pool(*r,value.before.bosses,request.boss_placement.region,pool,error)||
       !boss_current::prepare_birth(current,binding,pool->scope.manager,request.action_uuid,0,value.birth,error))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,std::move(value)).first->second;
    auto unknown=[&](const std::string& cause){error=op.cause=cause.empty()?"Boss native operation incomplete without a cause":cause;return action::Outcome::Unknown;};
    if(authority&&op.before.value.random!=authority->boss_placement_before->random){
        const auto result=boss_placement_calls::seed(op.before.random,op.before.value.random,authority->boss_placement_before->random,op.seed_attempted);
        if(result)return unknown("Boss Client authority RNG seed SEH/guard="+std::to_string(result));
        Sample seeded;
        if(!sample(*r,current,binding,born,seeded,error)||!same_binding(op.before,seeded)||seeded.money!=op.before.money||seeded.undo!=op.before.undo||
           seeded.value.untouched_sha!=op.before.value.untouched_sha||seeded.value.release!=op.before.value.release||
           seeded.value.random!=authority->boss_placement_before->random)return unknown(error.empty()?"Boss Client actual authority RNG seed readback differs":error);
        op.before=std::move(seeded);
    }
    op.evidence.before=op.before.money;op.evidence.boss_placement_before=op.before.value;Execution backend{*r,op};
    if(!boss_placement_transaction::execute(backend,request.boss_placement,op.native,error)){
        if(!op.seed_attempted&&!op.native.attempted){op.rejected_no_effect=true;op.cause=error;return action::Outcome::RejectedNoEffect;}
        const auto original_cause=error;
        if(op.native.purchase_returned&&!op.native.purchased&&!op.native.region_returned){
            Sample unchanged;
            if(sample(*r,current,binding,born,unchanged,error)&&same_binding(op.before,unchanged)&&unchanged.value==op.before.value&&
               unchanged.money==op.before.money&&unchanged.undo==op.before.undo){op.native.unknown=false;op.rejected_no_effect=true;op.cause=original_cause+"; complete no-effect readback confirmed";error=op.cause;return action::Outcome::RejectedNoEffect;}
            return unknown(original_cause+"; no-effect readback failed: "+(error.empty()?"native state differs":error));
        }
        return unknown(error.empty()?op.native.cause:error);
    }
    // The new local actor exists before the one shared physical actor writer.
    // Retain only its original Record once here; ACKs use current native rows.
    if(!definition(*r,current,binding,request.boss_placement.type,character,error)||character!=request.boss_placement.character_sha)
        return unknown(error.empty()?"Boss original core changed its authored Character definition":error);
    if(!sample(*r,current,binding,born,op.after,error))return unknown(error);
    if(authority){
        actors::State wanted;if(!boss_placement_state::adoption(op.after.actors,*authority->boss_placement_after,wanted,error)||
           !actors::adopt_current(current,binding,wanted,op.adoption,error)||!sample(*r,current,binding,born,op.after,error))return unknown(error);
    }
    if(!same_binding(op.before,op.after)||op.after.undo!=op.before.undo)
        return unknown("Boss original core changed its World or original Undo graph");
    op.evidence.after=op.after.money;op.evidence.boss_placement_after=op.after.value;
    op.evidence.undo_confirmed=op.evidence.selection_confirmed=true;
    if(!action::valid_boss_placement_effect(request,op.evidence)||(authority&&(op.evidence.before!=authority->before||op.evidence.after!=authority->after||
       op.evidence.boss_placement_before!=authority->boss_placement_before||op.evidence.boss_placement_after!=authority->boss_placement_after)))
        return unknown("Boss actual original complete result differs from Host or native purchase contract");
    trim(op.before);trim(op.after);op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||r->thread!=GetCurrentThreadId()||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return q::fail(error,"Boss observation lacks its exact confirmed original core transaction");
    const auto& op=found->second;Sample actual;
    if(!sample(*r,op.context,binding,boss_placement_state::key(request.action_uuid),actual,error,false,op.after.value.record_sha)||
       !same_binding(op.after,actual)||actual.value!=op.after.value||actual.money!=op.after.money||actual.undo!=op.after.undo)
        return q::fail(error,error.empty()?"Boss confirmed native identity/current actors/Ledger changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence actual;if(!observe(binding,request,actual,error))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return q::fail(error,"Boss original core transaction already retired");op.retired=true;return true;
}
void retire_session(){
    if(captured)reject("Boss captured placement cancelled after its source session retired");captured.reset();
    for(auto it=operations.begin();it!=operations.end();){
        if((it->second.native.attempted||it->second.seed_attempted)&&!it->second.confirmed&&!it->second.rejected_no_effect){trim(it->second.before);trim(it->second.after);auto row=operations.extract(it++);quarantined.insert(std::move(row));}
        else it=operations.erase(it);
    }
}
json status(){
    json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.native.attempted},{"purchased",op.native.purchased},
        {"purchase_returned",op.native.purchase_returned},{"region_returned",op.native.region_returned},{"add_returned",op.native.add_returned},{"birth_confirmed",op.birth.confirmed},
        {"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"actor_adoption_attempted",op.adoption.attempted},
        {"rejected_no_effect",op.rejected_no_effect},{"unknown",op.native.unknown||op.birth.unknown||op.adoption.unknown},{"cause",op.cause}};
    return {{"installed",installed},{"pending",pending()},{"captures",captures},{"submitted",submissions},{"rejected",rejections},{"forwarded",forwarded},
        {"cause",capture_cause},{"operations",std::move(rows)},{"quarantined",quarantined.size()}};
}
}
