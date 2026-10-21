namespace remove_diagnostics {
bool party_identity_bindings(json& out,std::string& error){return identity_refund_fixture::party_identity_bindings(out,error);}
bool party_identity_current(const std::string& snapshot,const std::string& key,uint64_t uid,uintptr_t pointer,uintptr_t stamp,std::string& error){
    return identity_refund_fixture::party_identity_current(snapshot,key,uid,pointer,stamp,error);
}
}
struct PartyOwnerFixture {
#include "party_owner_type_actual.inc"
    struct Context {std::string snapshot_uuid=::uuid;} context_;
    uintptr_t image_=fixture_image;
    std::map<std::string,Owner> owners_;
    std::map<uintptr_t,std::string> keys_;
    static bool fail(std::string& error,const std::string& cause){error=cause;return false;}
    bool current(std::string& error){World world;return identity_refund_fixture::current(world,error);}
    static uint32_t find_uid(uintptr_t,uint64_t uid,uintptr_t& found){
        void* pointer=nullptr;uint32_t seh=0;
        if(!lookup({},uid,pointer,seh))return seh?seh:1;
        found=reinterpret_cast<uintptr_t>(pointer);return seh;
    }
    template<class T>bool at(uintptr_t pointer,size_t offset,T& value,std::string& error){
        host::ProcessMemory memory;return memory.copy(pointer+offset,&value,sizeof value,error);
    }
#include "party_owner_methods_actual.inc"
};
void party_owner_checks(){
    std::string error;json aliases;
    const auto setup=[](){
        const auto canonical=natural_setup();auto& saved=registry.at(registry_id(::uuid,canonical));
        saved.scene_manager=0x71000000;saved.scene_region=fixture_saved_region;saved.scene_shard=fixture_root;
        return canonical;
    };
    auto canonical=setup();saved_owner_failure=true;
    need(party_identity_bindings(aliases,error)&&aliases.size()==4,
         "Party alias collector lost natural Scenery or other canonical owners");
    need(settlement_reads[101]==0&&saved_prop_reads==0&&prop_reads==0&&refund_calls==0,
         "Party alias collector read unused natural Scenery metadata or refunds");
    for(uint64_t uid=102;uid<=104;++uid)need(settlement_reads[uid]==1,
         "Party alias collector skipped full metadata of a non-excluded prop");
    const auto natural=std::find_if(aliases.begin(),aliases.end(),[&](const auto& row){return row.at("record_key")==canonical;});
    need(natural!=aliases.end()&&natural->at("local_uid")==101&&natural->at("object")==object(101)&&natural->at("stamp")==fixture_image+0x16c1390,
         "Party alias collector did not retain exact admitted UID/object/class");
    canonical=setup();registry.at(registry_id(::uuid,canonical)).scene_record.clear();
    need(party_identity_bindings(aliases,error)&&settlement_reads[101]==1,
         "Party alias collector omitted metadata for an unclassified Scenery row");
    canonical=setup();identity_failure=102;error.clear();
    need(!party_identity_bindings(aliases,error)&&error=="fixture live building identity failed"&&aliases.empty(),
         "Party alias collector hid non-excluded metadata failure or published partial aliases");
    for(unsigned fault=0;fault<9;++fault){
        canonical=setup();auto& saved=registry.at(registry_id(::uuid,canonical));error.clear();
        if(fault==0)saved.unknown=true;
        if(fault==1){auto duplicate=saved;duplicate.key+="duplicate";registry.emplace(registry_id(::uuid,duplicate.key),duplicate);}
        if(fault==2)++saved.world.epoch;
        if(fault==3){auto duplicate=saved;duplicate.uid=999;duplicate.key+="duplicate";registry.emplace(registry_id(::uuid,duplicate.key),duplicate);}
        if(fault==4)lookup_override=object(102);
        if(fault==5)saved.object+=8;
        if(fault==6)saved.class_addresspoint+=8;
        if(fault==7)change_world_on_lookup=true;
        if(fault==8)unknown_locked=true;
        need(!party_identity_bindings(aliases,error)&&!error.empty()&&aliases.empty(),
             "Party alias collector accepted Unknown, ambiguity, foreign World, changed UID/object or class");
    }
    canonical=setup();registry.at(registry_id(::uuid,canonical)).revoked=true;
    need(party_identity_bindings(aliases,error)&&aliases.size()==3&&std::none_of(aliases.begin(),aliases.end(),[&](const auto& row){return row.at("record_key")==canonical;}),
         "Party alias collector retained a revoked target");
    canonical=setup();host::batch_final_failure="original final Party alias World proof failed";error.clear();
    need(!party_identity_bindings(aliases,error)&&error==host::batch_final_failure&&aliases.empty(),
         "Party alias collector published before final complete World proof");
    host::batch_final_failure.clear();
    canonical=setup();PartyOwnerFixture party;
    need(party.prop_pins(party.owners_,party.keys_,error)&&party.owners_.size()==4&&party.keys_.size()==4,
         "actual Party owner merge lost complete prop aliases");
    const auto owner=party.owners_.at(canonical);
    need(owner.prop&&owner.uid==101&&owner.object==object(101),"actual Party owner merge lost expected full UID");
    fixture_saves=1;const auto prior_settlement=settlement_reads;
    need(party.checked(owner,error)&&settlement_reads==prior_settlement,
         "actual Party held callback invoked idle-only metadata resolver or rejected admitted Scenery");
    LocalTarget idle_target;error.clear();
    need(!resolve_identity_destination(object(101),idle_target,error)&&error=="info: remove_wait_save_edit_or_graph",
         "held Party fixture failed to enforce the real idle-only resolver boundary");
    for(unsigned fault=0;fault<13;++fault){
        canonical=setup();PartyOwnerFixture actual;
        need(actual.prop_pins(actual.owners_,actual.keys_,error),"Party reference fixture admission failed");
        const auto admitted=actual.owners_.at(canonical);fixture_saves=1;
        auto& saved=registry.at(registry_id(::uuid,canonical));error.clear();
        if(fault==0)saved.revoked=true;
        if(fault==1)saved.unknown=true;
        if(fault==2)++registry_generation;
        if(fault==3)++live_world.world_epoch;
        if(fault==4)saved.uid=102;
        if(fault==5)saved.object+=8;
        if(fault==6)saved.class_addresspoint+=8;
        if(fault==7)lookup_override=object(102);
        if(fault==8)actual.context_.snapshot_uuid="foreign";
        if(fault==9)actual.keys_.clear();
        if(fault==10)scene_target=false;
        if(fault==11)live.erase(101);
        if(fault==12)unknown_locked=true;
        need(!actual.checked(admitted,error)&&!error.empty(),
             "actual held Party reference accepted revoked, Unknown or changed canonical lifetime");
    }
    canonical=setup();PartyOwnerFixture conflict;
    conflict.keys_[object(101)]="other";error.clear();
    need(!conflict.prop_pins(conflict.owners_,conflict.keys_,error)&&error=="Party prop alias conflicts with a current external owner",
         "actual Party alias merge accepted another canonical key for the same pointer");
    canonical=setup();PartyOwnerFixture existing;
    existing.keys_[object(101)]=canonical;
    existing.owners_[canonical]={object(101),fixture_image+0x16c1390,0,false};
    need(existing.prop_pins(existing.owners_,existing.keys_,error)&&existing.owners_.at(canonical).uid==101&&existing.owners_.at(canonical).prop,
         "actual Party merge rejected the same nonprop census identity or failed to retain its prop UID");
    canonical=setup();need(identity_bindings(aliases,error)&&settlement_reads[101]==1,
         "Party-specific exclusion changed the full default identity collector");
    reset();
}
