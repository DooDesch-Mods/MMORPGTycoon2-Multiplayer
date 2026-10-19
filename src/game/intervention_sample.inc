namespace {
namespace intervention=interventions;
struct InterventionSample {Local local;intervention::Census census;intervention::Actor selected;a::InterventionProof proof;a::Ledger money;};
bool intervention_digest(const nlohmann::json& value,std::string& out,std::string& error){try{const auto bytes=value.dump();out=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}catch(const std::exception& exception){return fail(error,std::string("Intervention complete current value serialization: ")+exception.what());}}
bool intervention_sample(Runtime& r,const a::Context& context,const a::Binding& binding,const intervention::Target& target,InterventionSample& out,std::string& error){
    out={};if(!intervention::valid(target))return fail(error,"Intervention target family/key invalid");management_simulation::LiveState state;
    if(!primary_read(r,context,binding,out.local,state,error)||!intervention::census(context,binding,out.census,error))return false;
    auto select=[&](){const auto found=std::find_if(out.census.actors.begin(),out.census.actors.end(),[&](const auto& actor){return actor.target==target;});
        if(found==out.census.actors.end())return fail(error,"Intervention target absent from complete current original Actor census");out.selected=*found;return true;};
    if(!select())return false;out.proof.target=target;
    auto external=[&](uintptr_t object,std::string& key,std::string& cause){const auto found=out.census.owners.find(object);if(found==out.census.owners.end())return fail(cause,"Intervention original Record references an unresolved actual external owner");key=found->second;return true;};
    if(target.family==intervention::Family::Toon){const auto before=out.census.actors;intervention_records::Subscriber subscriber;
        int32_t id=0;for(const auto c:std::string_view(target.key).substr(11))id=id*10+(c-'0');
        if(!intervention_records::subscriber(context,binding,id,external,subscriber,error))return false;out.proof.subscriber=subscriber.record.portable;
        intervention::Census current;if(!intervention::census(context,binding,current,error))return false;
        if(current.actors!=before)return fail(error,"Intervention original Subscriber record capture changed complete Actor membership/identity");out.census=std::move(current);if(!select())return false;}
    nlohmann::json actors=nlohmann::json::array(),zones=nlohmann::json::array();
    for(const auto& actor:out.census.actors){npc_probe::character_actor_records::Result record;
        if(!npc_probe::character_actor_records::read(binding,context.snapshot_uuid,character_membership_reader::Family(uint8_t(actor.target.family)),actor.object,external,record,error))return false;
        actors.push_back({{"target",intervention::encode(actor.target)},{"record",intervention::encode(record.portable)}});if(actor.target==target)out.proof.actor=record.portable;}
    for(const auto& zone:out.census.zones){npc_probe::character_actor_records::Result record;
        if(!intervention_records::zone(context,binding,zone.object,zone.uid,zone.key,external,record,error))return false;
        zones.push_back({{"key",zone.key},{"record",intervention::encode(record.portable)}});}
    nlohmann::json economy,direct_monsters,marketing;
    if(!typed_action_runtime::read_intervention_live(context,binding,economy,direct_monsters,marketing,error))return false;
    // Native tags are local manager counters. Existing live Monster identity and
    // all actual combat/lifecycle/animation values remain in this witness.
    for(auto& row:direct_monsters.at("rows"))row.erase("tag");
    gm_ticket::Publication gm;if(!gm_capture(r,context,binding,out.local,gm,error))return false;tutorial_replication::State tutorial;
    if(!tutorial_replication::capture(context,binding,tutorial,error)||!intervention_digest({{"records",actors},{"direct_live",direct_monsters}},out.proof.actors_sha,error)||
       !intervention_digest(zones,out.proof.zones_sha,error)||!intervention_digest(economy,out.proof.economy_sha,error)||!intervention_digest(gm_ticket::encode(gm),out.proof.gm_sha,error)||
       !intervention_digest(marketing,out.proof.marketing_sha,error)||!intervention_digest(tutorial_replication::encode(tutorial),out.proof.tutorial_sha,error))return false;
    management_simulation::LiveState final;Local current;if(!primary_read(r,context,binding,current,final,error))return false;
    if(current.pin!=out.local.pin||current.software!=out.local.software||current.release!=out.local.release)return fail(error,"Intervention complete original capture changed native roots/Clock/full Ledger");
    intervention::Census after;if(!intervention::census(context,binding,after,error))return false;
    if(after.actors!=out.census.actors||after.zones!=out.census.zones)return fail(error,"Intervention complete original capture changed actual Actor/Zone identities");
    out.money=live::ledger(current.pin.values);return intervention::valid(out.proof)||fail(error,"Intervention complete original native record/cascade proof invalid");
}
}
