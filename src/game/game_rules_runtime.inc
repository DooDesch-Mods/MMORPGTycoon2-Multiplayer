namespace {
namespace rules=game_rules;
struct RulesRoute {uintptr_t rva;size_t bytes,stolen;const char* sha;Ui original=nullptr;release_ui_owner::Owner owner;};
struct RulesPending {a::Context context;a::Binding binding;a::GameRulesIntent intent;std::string id;};
struct RulesRuntime {std::array<RulesRoute,3> routes{{
#include "game_rules_route_pins.inc"
    }};std::optional<RulesPending> pending;std::map<std::string,Receipt> operations;
};
RulesRuntime design_rules;
bool rules_routes(Runtime& r,std::string& error){for(const auto& route:design_rules.routes){const auto& owner=route.owner;MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
    if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"Design rule route read original="+hooks::framework::process_backend().error());
    if(actual.state!=MT2FW_MT2_ROUTE_ENABLED||actual.generation!=owner.generation||actual.entry!=owner.entry||actual.trampoline!=owner.trampoline||actual.detour!=owner.detour||std::strcmp(actual.owner,"doodesch.multiplayer"))return fail(error,"Design rule physical route owner/generation differs");
    if(!release_ui_owner::verify(owner,[&](auto address,auto out,auto bytes,auto& cause){return r.memory.copy(address,out,bytes,cause);},[](const auto& bytes){return mt2mp::sha256(bytes);},route.sha,error)||!hooks::framework::process_backend().verify_code(owner.entry,owner.pristine.data(),owner.pristine.size(),error))return false;}
    struct Code{uintptr_t rva;size_t bytes;const char* sha;};const Code code[]{
#include "game_rules_helper_pins.inc"
    };
    for(const auto& pin:code){std::vector<unsigned char> bytes(pin.bytes);if(!r.memory.copy(r.image+pin.rva,bytes.data(),bytes.size(),error)||mt2mp::sha256(bytes)!=pin.sha)return fail(error,error.empty()?"Design rule original helper body differs RVA="+std::to_string(pin.rva):error);}return true;
}
bool rules_once(Runtime& r,rules::Pin& pin,rules::State& value,std::string& error){
    auto read=[&](uintptr_t object,size_t offset,auto& out,std::string& cause){return at(r,object,offset,out,cause);};
    auto copy=[&](uintptr_t address,void* out,size_t size,std::string& cause){return r.memory.copy(address,out,size,cause);};
    auto name=[&](uintptr_t object,std::string& out,std::string& cause){return text(r,object,out,200,cause);};
    auto find=[&](uintptr_t tree,const std::string& key,uintptr_t& out,std::string& cause){const auto exception=rules::native::find(r.image,tree,key,out);return !exception||fail(cause,"Design rule original GetTech SEH="+std::to_string(exception));};
    return rules::read(read,copy,name,find,r.image,pin,value,error);
}
bool rules_values(Runtime& r,rules::State& value,rules::Pin& pin,std::string& error){if(GetCurrentThreadId()!=r.thread)return fail(error,"Design native rule read requires saved Mainthread");rules::State first;rules::Pin previous;
    if(!rules_once(r,previous,first,error)||!rules_once(r,pin,value,error))return false;return (previous==pin&&first==value)||fail(error,"Design current native owners/values changed during double read");
}
bool rules_proof(Runtime& r,const a::Context& context,const a::Binding& binding,Local& local,rules::Pin& pin,a::GameRulesProof& proof,a::Ledger& ledger,std::string& error){
    management_simulation::LiveState state;if(!primary_read(r,context,binding,local,state,error)||!rules_values(r,proof.state,pin,error)||pin.software!=local.software)return fail(error,error.empty()?"Design current Software/Clock binding differs":error);
    tutorial_replication::State tutorial;if(!tutorial_replication::capture(context,binding,tutorial,error))return false;const auto encoded=tutorial_replication::encode(tutorial).dump();proof.tutorial_sha256=mt2mp::sha256(std::vector<unsigned char>(encoded.begin(),encoded.end()));ledger=live::ledger(local.pin.values);
    rules::State checked;rules::Pin current;if(!rules_values(r,checked,current,error)||checked!=proof.state||current!=pin)return fail(error,error.empty()?"Design native owners/values changed during complete tutorial capture":error);return rules::valid(proof)||fail(error,"Design complete native proof invalid");
}
bool rules_queue(Runtime& r,const rules::Intent& intent,std::string& error){
    if(!rules::valid(intent))return fail(error,"Design requested native rule change invalid");const auto controller=typed_action_runtime::controller_status();a::Context context;a::Binding binding;
    if(!r.ready||r.replay||GetCurrentThreadId()!=r.thread||r.pending||policy_pending()||design_rules.pending||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!a::selected_session().current(context,error)||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||!rules_routes(r,error))return fail(error,error.empty()?"Design normal UI requires idle bound Controller":error);
    Local local;rules::Pin pin;a::GameRulesProof before;a::Ledger money;if(!rules_proof(r,context,binding,local,pin,before,money,error))return false;if(before.state!=intent.before)return fail(error,"Design captured complete before state differs");
    auto id=uuid(error);if(id.empty())return false;design_rules.pending=RulesPending{context,binding,intent,id};++r.captures;return true;
}
bool rules_active(Runtime& r){return !r.replay&&net::session_wire::connected()&&typed_action_runtime::controller_status().value("started",false);}
bool rules_matches(Runtime& r,void* record,const char* name,bool& matches,std::string& error){const auto exception=rules::native::command(r.image,record,name,matches);return !exception||fail(error,"Design original command read SEH="+std::to_string(exception));}
bool rules_xp_entry(void* receiver,void* record){auto* r=published.load(std::memory_order_acquire);const auto original=design_rules.routes[0].original;
    if(!r||!original)throw std::runtime_error("Design original XP command was not published");if(!rules_active(*r))return original(receiver,record);
    std::string error;bool matches=false;if(!rules_matches(*r,record,"Apply",matches,error)){reject(*r,error);return true;}if(!matches)return original(receiver,record);
    if(!ap(*r,uintptr_t(receiver),0x1703ce0,error)||!ap(*r,uintptr_t(receiver)+0x338,0x16d9f40,error)){reject(*r,error);return true;}
    rules::Intent intent;rules::Pin pin;if(!rules_values(*r,intent.before,pin,error)||!at(*r,uintptr_t(receiver),0x350,intent.after.xp,error)){reject(*r,error);return true;}
    const auto desired=intent.after.xp;intent.after=intent.before;intent.after.xp=desired;intent.command=rules::Command::ApplyXP;
    if(!rules_queue(*r,intent,error)){reject(*r,error);return true;}
    const auto exception=rules::native::buttons(r->image,receiver,false);if(exception){design_rules.pending.reset();reject(*r,"Design original XP button consumption SEH="+std::to_string(exception)+"; gameplay not submitted");}return true;
}
bool rules_property_entry(void* receiver,void* record){auto* r=published.load(std::memory_order_acquire);const auto original=design_rules.routes[1].original;
    if(!r||!original)throw std::runtime_error("Design original property command was not published");if(!rules_active(*r))return original(receiver,record);
    std::string error;bool matches=false;if(!rules_matches(*r,record,"ValueEdited",matches,error)){reject(*r,error);return true;}if(!matches)return original(receiver,record);
    uintptr_t value=0,owner=0,design=0;if(!at(*r,uintptr_t(receiver),0x450,value,error)){reject(*r,error);return true;}if(!value)return original(receiver,record);
    if(!at(*r,value,8,owner,error)||!at(*r,r->image,0x13df9e0,design,error)){reject(*r,error);return true;}if(!design||owner!=design+0x1c0)return original(receiver,record);
    uintptr_t scalar=0,edit=0;std::string name,source;if(!ap(*r,uintptr_t(receiver),0x1701d20,error)||!ap(*r,value,0x17496b0,error)||!at(*r,value,0x38,scalar,error)||scalar!=owner+0x18||!text(*r,value+0x18,name,32,error)||name!="seconds"||!at(*r,uintptr_t(receiver),0x458,edit,error)||!ap(*r,edit,0x1703290,error)||!text(*r,edit+0x2d0,source,128,error)){reject(*r,error.empty()?"Design Respawn bound property owner/name/edit pane differs":error);return true;}
    int32_t desired=0;bool parsed=false;const auto exception=rules::native::parse(r->image,source,desired,parsed);if(exception){reject(*r,"Design original property integer parse SEH="+std::to_string(exception));return true;}if(!parsed)return original(receiver,record);
    rules::Intent intent;rules::Pin pin;if(!rules_values(*r,intent.before,pin,error)){reject(*r,error);return true;}if(desired==intent.before.respawn_seconds)return original(receiver,record);intent.after=intent.before;intent.after.respawn_seconds=desired;intent.command=rules::Command::RespawnSeconds;
    if(!rules_queue(*r,intent,error))reject(*r,error);return true;
}
bool rules_features_entry(void* receiver,void* record){auto* r=published.load(std::memory_order_acquire);const auto original=design_rules.routes[2].original;
    if(!r||!original)throw std::runtime_error("Design original Features command was not published");if(!rules_active(*r))return original(receiver,record);
    std::string error;bool matches=false;if(!rules_matches(*r,record,"Features",matches,error)){reject(*r,error);return true;}if(!matches)return original(receiver,record);
    if(!ap(*r,uintptr_t(receiver),0x16ff590,error)){reject(*r,error);return true;}rules::Intent intent;rules::Pin pin;if(!rules_values(*r,intent.before,pin,error)){reject(*r,error);return true;}if(!rules::feature_calls(intent.before))return original(receiver,record);intent.after=intent.before;intent.command=rules::Command::Features;
    for(size_t i=0;i<2;++i){uintptr_t checkbox=0;uint8_t checked=0;if(!at(*r,uintptr_t(receiver),0x318+i*8,checkbox,error)||!ap(*r,checkbox,0x16d98c0,error)||!at(*r,checkbox,0x346,checked,error)||checked>1){reject(*r,error.empty()?"Design original complete Features controls absent or invalid":error);return true;}if(intent.before.available[i])intent.after.features[i]=checked;}
    if(!rules_queue(*r,intent,error))reject(*r,error);return true;
}
bool rules_checkbox(Runtime& r,uintptr_t receiver,bool desired,bool& handled,std::string& error){handled=false;uintptr_t value=0,owner=0,design=0;
    if(!at(r,receiver,0x3c8,value,error))return false;if(!value)return true;if(!at(r,value,8,owner,error)||!at(r,r.image,0x13df9e0,design,error))return false;if(!design||owner!=design+0x1c0)return true;handled=true;
    std::string name;uintptr_t scalar=0;if(!ap(r,receiver,0x16d98c0,error)||!ap(r,value,0x1749610,error)||!text(r,value+0x18,name,32,error)||name!="delayForToons"||!at(r,value,0x38,scalar,error)||scalar!=owner+0x1c)return fail(error,error.empty()?"Design native Respawn Boolean binding differs":error);
    rules::Intent intent;rules::Pin pin;if(!rules_values(r,intent.before,pin,error))return false;if(intent.before.respawn_delay==desired){handled=false;return true;}intent.after=intent.before;intent.after.respawn_delay=desired;intent.command=rules::Command::RespawnDelay;return rules_queue(r,intent,error);
}
bool rules_enable(Runtime& r,std::string& error){const Ui entries[]{&rules_xp_entry,&rules_property_entry,&rules_features_entry};
    for(size_t i=0;i<design_rules.routes.size();++i){auto& route=design_rules.routes[i];auto& owner=route.owner;owner.entry=r.image+route.rva;owner.detour=uintptr_t(entries[i]);owner.stolen=route.stolen;owner.pristine.resize(route.bytes);
        if(!r.memory.copy(owner.entry,owner.pristine.data(),owner.pristine.size(),error)||mt2mp::sha256(owner.pristine)!=route.sha)return fail(error,error.empty()?"Design original complete command body differs":error);
        const auto created=MH_CreateHook(reinterpret_cast<void*>(owner.entry),reinterpret_cast<void*>(entries[i]),reinterpret_cast<void**>(&route.original));if(created!=MH_OK||!route.original)return fail(error,"Design named route prepare failed status="+std::to_string(created)+" original="+hook_cause());owner.trampoline=uintptr_t(route.original);
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(owner.entry));if(enabled!=MH_OK)return fail(error,"Design named route enable failed status="+std::to_string(enabled)+" original="+hook_cause());MT2FW_MT2_RouteReceipt actual{};actual.size=sizeof actual;
        if(!hooks::framework::process_backend().read_route(reinterpret_cast<void*>(owner.entry),actual))return fail(error,"Design named route receipt failed original="+hook_cause());owner.generation=actual.generation;
    }return rules_routes(r,error);
}
bool rules_pending(){return design_rules.pending.has_value();}
void rules_reset(){design_rules.pending.reset();design_rules.operations.clear();}
void rules_tick(Runtime& r){if(!design_rules.pending)return;const auto pending=*design_rules.pending;design_rules.pending.reset();std::string error;a::Context context;a::Binding binding;
    if(!a::selected_session().current(context,error)||context!=pending.context||!typed_action_runtime::network_binding(context.snapshot_uuid,binding,error)||binding!=pending.binding||!typed_action_runtime::submit_game_rules(pending.id,pending.intent,error)){reject(r,error.empty()?"Design saved normal UI binding changed":error);return;}++r.submitted;
}
a::Outcome rules_perform(const a::Binding& binding,const a::Request& request,const a::Evidence* host,std::string& error){
    auto* r=published.load(std::memory_order_acquire);if(!r||!r->ready||request.action!=a::Action::GameRules||!rules::valid(request.game_rules)){error="Design runtime/action unavailable";return a::Outcome::RejectedNoEffect;}
    if(design_rules.operations.contains(request.action_uuid)){error="Design original token already attempted; no replay";return a::Outcome::Unknown;}
    if(design_rules.operations.size()>=a::history_limit){auto oldest=design_rules.operations.end();for(auto it=design_rules.operations.begin();it!=design_rules.operations.end();++it)if(it->second.retired&&(oldest==design_rules.operations.end()||it->second.request.host_revision<oldest->second.request.host_revision))oldest=it;
        if(oldest==design_rules.operations.end()){error="Design unretired original receipt capacity reached";return a::Outcome::RejectedNoEffect;}design_rules.operations.erase(oldest);}
    const auto controller=typed_action_runtime::controller_status();a::Context context;
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid||!a::selected_session().current(context,error)||context.snapshot_uuid!=request.snapshot_uuid||(context.role==a::Role::Client)!=(host!=nullptr)||!rules_routes(*r,error)){if(error.empty())error="Design original requires exact Controller/Context/role";return a::Outcome::RejectedNoEffect;}
    Local local;rules::Pin pin;a::GameRulesProof before;a::Ledger money;if(!rules_proof(*r,context,binding,local,pin,before,money,error))return a::Outcome::RejectedNoEffect;
    if(before.state!=request.game_rules.before){error="Design actual complete before rules differ from captured intent";return a::Outcome::RejectedNoEffect;}
    if(host&&(!host->game_rules_before||!host->game_rules_after||*host->game_rules_before!=before||host->before!=money||host->after!=money)){error="Design Client complete before rules/tutorial/Ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    live::NativePort port(context,binding,nullptr);if(!port.current_owners(local.pin,error))return a::Outcome::RejectedNoEffect;
    auto& receipt=design_rules.operations.emplace(request.action_uuid,Receipt{binding,request}).first->second;receipt.evidence.before=money;receipt.evidence.game_rules_before=before;
    r->replay=true;receipt.attempted=true;const auto native=rules::native::change(r->image,pin,request.game_rules);r->replay=false;receipt.returned=native.returned;
    auto unknown=[&](const std::string& cause){error=receipt.cause=cause;return a::Outcome::Unknown;};
    if(native.exception||!native.returned)return unknown("Design original rule application/cascade failed SEH="+std::to_string(native.exception)+"; no replay");
    if(request.game_rules.command==rules::Command::Features&&(native.feature_calls!=rules::feature_calls(before.state)||native.feature_events!=rules::feature_events(before.state)))return unknown("Design complete original Features0/1 calls/FeatureEnabled event sequence differs");
    Local final;rules::Pin after_pin;a::GameRulesProof after;if(!rules_proof(*r,context,binding,final,after_pin,after,receipt.evidence.after,error))return unknown(error);receipt.evidence.game_rules_after=after;
    if(final.pin!=local.pin||after_pin!=pin||!rules::effect(request.game_rules,before,after)||receipt.evidence.after!=money||(host&&*host->game_rules_after!=after))return unknown("Design complete original rules/tutorial/owner/Clock/Ledger readback differs from intent/Host");
    receipt.confirmed=true;error.clear();return a::Outcome::Confirmed;
}
bool rules_observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){auto* r=published.load(std::memory_order_acquire);if(!r)return fail(error,"Design receipt owner absent");const auto found=design_rules.operations.find(request.action_uuid);
    if(found==design_rules.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(error,"Design exact confirmed original receipt absent");
    Local local;rules::Pin pin;a::Context context;a::GameRulesProof actual;a::Ledger ledger;if(!a::selected_session().current(context,error))return false;if(context.snapshot_uuid!=request.snapshot_uuid)return fail(error,"Design receipt current saved snapshot differs");
    if(!rules_proof(*r,context,binding,local,pin,actual,ledger,error))return false;if(actual!=*found->second.evidence.game_rules_after||ledger!=found->second.evidence.after)return fail(error,"Design actual complete rules/tutorial/Ledger differs before receipt retirement");out=found->second.evidence;out.retirement_confirmed=found->second.retired;error.clear();return true;
}
bool rules_retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence actual;if(!rules_observe(binding,request,actual,error))return false;design_rules.operations.at(request.action_uuid).retired=true;error.clear();return true;}
bool rules_adopt(Runtime& r,const rules::State& desired,live::ManagementReceipt& receipt,std::string& error){
    rules::State before;rules::Pin pin;if(!rules::valid(desired)||!rules_values(r,before,pin,error))return fail(error,error.empty()?"Design authoritative values invalid":error);
    if(before.available!=desired.available)return fail(error,"Design required current technology ownership differs; original project transaction missing");
    const auto store=[&](uintptr_t address,unsigned bytes,uint32_t old,uint32_t target){if(old==target)return true;rules::State actual;rules::Pin current;
        if(!rules_values(r,actual,current,error)||current!=pin||actual!=before)return fail(error,error.empty()?"Design authoritative adoption concurrent owner/value differs":error);bool stored=false;receipt.attempted=true;++receipt.attempted_stores;
        const auto exception=gn::compare(address,bytes,old,target,stored);if(exception||!stored)return fail(error,"Design authoritative field CAS failed SEH="+std::to_string(exception));++receipt.stores;return true;};
    for(size_t i=0;i<14;++i){if(!store(pin.design+0x188+i*4,4,before.xp[i],desired.xp[i]))return false;before.xp[i]=desired.xp[i];}
    if(!store(pin.design+0x1d8,4,uint32_t(before.respawn_seconds),uint32_t(desired.respawn_seconds)))return false;before.respawn_seconds=desired.respawn_seconds;
    if(!store(pin.design+0x1dc,1,uint32_t(before.respawn_delay),uint32_t(desired.respawn_delay)))return false;before.respawn_delay=desired.respawn_delay;
    for(size_t i=0;i<2;++i){if(!store(pin.features+0x18+i,1,before.features[i],desired.features[i]))return false;before.features[i]=desired.features[i];}return true;
}
}
