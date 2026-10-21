void building_identity_checks(){
    std::string error;std::vector<LocalTarget> owners;std::map<uintptr_t,std::string> destinations;
    const auto setup=[&](bool only_scenery){
        const auto canonical=natural_setup();auto& saved=registry.at(registry_id(::uuid,canonical));
        saved.scene_manager=0x71000000;saved.scene_region=fixture_saved_region;saved.scene_shard=fixture_root;
        if(only_scenery){for(uint64_t uid=102;uid<=104;++uid){registry.erase(registry_id(::uuid,key(uid)));live.erase(uid);}}
        return canonical;
    };
    auto canonical=setup(true);
    const std::set<uint64_t> omitted{101};json compact,complete;
    auto registry_before=registry;auto entry_before=next_entry;
    captured.bindings_confirmed=false;fixture_loaded.state="not-confirmed";
    need(collect_bindings(compact,error,BindingDetails::BuildingTargets,&omitted)&&compact.at("bindings").empty()&&
         !compact.contains("baseline_snapshot_id")&&seals==0&&registry==registry_before&&next_entry==entry_before&&scene_scenery::append_calls==1,
         "all-Scenery compact census reimported the snapshot or skipped pending admission");
    need(settlement_reads.empty()&&refund_calls==0,"compact omitted row performed native metadata/refund work");
    canonical=setup(false);
    need(collect_bindings(compact,error,BindingDetails::BuildingTargets,&omitted)&&compact.at("bindings").size()==3,
         "compact Building census materialized an already-excluded saved Scenery row");
    need(collect_bindings(complete,error,BindingDetails::Identity,&omitted)&&complete.at("bindings").size()==4,
         "compact mode changed complete Identity/Party report shape");
    auto expected=complete;auto& expected_rows=expected.at("bindings");
    expected_rows.erase(std::remove_if(expected_rows.begin(),expected_rows.end(),[](const auto& row){return row.at("local_uid")==101;}),expected_rows.end());
    need(compact==expected,"compact census changed a retained row or complete World fields");
    canonical=setup(false);
    need(collect_bindings(compact,error,BindingDetails::BuildingTargets)&&compact.at("bindings").size()==4,
         "absent admitted exclusion set skipped an unclassified row");
    for(const char* field:{"type","variant","x","y","z"}){
        canonical=setup(true);registry.at(registry_id(::uuid,canonical)).metadata.erase(field);bool caught=false;
        try{collect_bindings(compact,error,BindingDetails::BuildingTargets,&omitted);}
        catch(const json::out_of_range&){caught=true;}
        need(caught,"compact omitted row suppressed its original stored-field guard");
    }
    canonical=setup(true);auto& unknown=registry.at(registry_id(::uuid,canonical));unknown.revoked=unknown.unknown=true;
    need(!identity_building_targets(owners,error,&destinations)&&error=="Service census includes Unknown native binding"&&owners.empty()&&destinations.empty(),
         "compact mode concealed an Unknown revoked row");
    canonical=setup(true);
    scene_scenery::append_fixture=[](json& rows,std::string& cause){
        const auto added=sealed.buildings.at(1);live.emplace(added.local_uid,added.building);
        return import_bindings(std::vector<Imported>{added},::uuid,::fixture_sha,admitted_world,rows,cause);
    };
    need(identity_building_targets(owners,error,&destinations)&&owners.size()==1&&owners.front().uid==102&&destinations.size()==2&&
         scene_scenery::append_calls==1&&seals==0&&settlement_reads[102]==1,
         "all-Scenery compact census lost a newly admitted Building or its fresh target proof");
    canonical=setup(true);scene_scenery::append_fixture=[](json&,std::string& cause){cause="original pending scene admission failed";return false;};
    need(!identity_building_targets(owners,error,&destinations)&&error=="original pending scene admission failed"&&owners.empty()&&destinations.empty(),
         "compact all-Scenery census suppressed pending admission failure");
    canonical=setup(true);scene_scenery::append_fixture=[](json&,std::string&){registry.begin()->second.scene_manager=0;return true;};
    need(!identity_building_targets(owners,error,&destinations)&&error=="Building owner census saved exclusion changed during collection"&&owners.empty()&&destinations.empty(),
         "compact census published after its admitted exclusion changed");
    canonical=setup(true);scene_scenery::append_fixture=[](json&,std::string&){++live_world.generation;return true;};
    need(!identity_building_targets(owners,error,&destinations)&&error=="fixture readonly batch complete World changed"&&owners.empty()&&destinations.empty(),
         "compact census published after its closing World changed");
    canonical=setup(true);saved_owner_failure=true;
    need(identity_building_targets(owners,error,&destinations)&&owners.empty()&&destinations==std::map<uintptr_t,std::string>{{object(101),canonical}},
         "Building-only census lost admitted Scenery destination candidates in an empty Building world");
    need(settlement_reads.empty()&&prop_reads==0&&saved_prop_reads==0&&refund_calls==0,
         "unused saved Root Scenery invoked an expensive property or refund reader");
    canonical=setup(false);error.clear();
    need(identity_building_targets(owners,error,&destinations)&&owners.size()==3&&destinations.size()==4,
         "Building-only census lost fresh Building owners or complete destination candidates");
    need(settlement_reads[101]==0&&saved_prop_reads==0&&prop_reads==3,
         "Building-only census skipped fresh Building proofs or read unused Root Scenery");
    for(uint64_t uid=102;uid<=104;++uid)need(settlement_reads[uid]==2,"fresh Building settlement/resolver proof was omitted");
    canonical=setup(false);identity_failure=104;error.clear();
    need(!identity_building_targets(owners,error,&destinations)&&error=="fixture live building identity failed"&&owners.empty()&&destinations.empty(),
         "Building-only census hid the original Building property failure");
    for(unsigned fault=0;fault<5;++fault){
        canonical=setup(true);auto& saved=registry.at(registry_id(::uuid,canonical));error.clear();
        if(fault==0)saved.unknown=true;
        if(fault==1){auto copy=saved;copy.key+="duplicate";registry.emplace(registry_id(::uuid,copy.key),copy);}
        if(fault==2)++saved.world.epoch;
        if(fault==3){auto copy=saved;copy.key+="duplicate";copy.uid=999;registry.emplace(registry_id(::uuid,copy.key),copy);}
        if(fault==4)world_failure=true;
        need(!identity_building_targets(owners,error,&destinations)&&!error.empty()&&owners.empty()&&destinations.empty(),
             "unused Scenery exclusion hid Unknown, duplicate UID/pointer, foreign World or original current failure");
    }
    canonical=setup(false);std::set<uintptr_t> proved;
    need(identity_building_targets(owners,error,&destinations),"Service preflight Building census failed");
    for(const auto& owner:owners)proved.insert(owner.object);
    const auto prior_saved_reads=saved_prop_reads;std::string destination;
    need(service_destination_actual(destinations,&proved,object(101),destination,error)&&destination==canonical&&saved_prop_reads==prior_saved_reads+1,
         "actual Service destination callback did not freshly prove referenced saved Root Scenery");
    const auto before_building=prop_reads;
    need(service_destination_actual(destinations,&proved,object(102),destination,error)&&destination==key(102)&&prop_reads==before_building,
         "actual Service callback repeated the already fresh Building owner proof");
    saved_owner_failure=true;error.clear();
    need(!service_destination_actual(destinations,&proved,object(101),destination,error)&&error=="original saved Root Scenery owner changed",
         "actual Service destination callback suppressed original saved owner failure");
    const auto pre_scope_reads=saved_prop_reads;error.clear();
    need(service_destination_actual(destinations,nullptr,object(101),destination,error)&&destination==canonical&&saved_prop_reads==pre_scope_reads,
         "SaveScope destination lookup attempted idle-only metadata admission");
    need(!service_destination_actual(destinations,nullptr,object(999),destination,error)&&error=="Service Frame destination lacks complete canonical Building identity",
         "SaveScope accepted an unproved new destination");
    canonical=setup(false);LocalTarget fixture_loaded_target;error.clear();
    const auto& fixture_saved_building=registry.at(registry_id(::uuid,key(102)));
    need(!fixture_saved_building.object&&!fixture_saved_building.class_addresspoint,
         "loaded Building fixture already has a pointer hint and cannot exercise original admission");
    need(identity_building_targets(owners,error,&destinations)&&
         resolve_identity_destination(object(102),fixture_loaded_target,error)&&fixture_loaded_target.object==object(102)&&
         fixture_loaded_target.uid==102&&fixture_loaded_target.record_key==key(102)&&fixture_loaded_target.normal_building,
         "loaded Building full UID did not resolve without an optional pointer hint");
    need(!registry.at(registry_id(::uuid,key(102))).object&&!registry.at(registry_id(::uuid,key(102))).class_addresspoint,
         "readonly loaded Building resolution created a mutable pointer admission");
    const std::set<uintptr_t> fixture_no_prior_proof;
    need(service_destination_actual(destinations,&fixture_no_prior_proof,object(102),destination,error)&&destination==key(102),
         "actual Service destination callback rejected the loaded Building full UID");
    for(unsigned fixture_fault=0;fixture_fault<9;++fixture_fault){
        canonical=setup(false);auto& fixture_binding=registry.at(registry_id(::uuid,key(102)));error.clear();
        if(fixture_fault==0)fixture_binding.object=object(103);
        if(fixture_fault==1)fixture_binding.revoked=true;
        if(fixture_fault==2)fixture_binding.unknown=true;
        if(fixture_fault==3){auto fixture_duplicate=fixture_binding;fixture_duplicate.key+="duplicate";
            registry.emplace(registry_id(::uuid,fixture_duplicate.key),fixture_duplicate);}
        if(fixture_fault==4)++fixture_binding.world.epoch;
        if(fixture_fault==5)lookup_override=object(103);
        if(fixture_fault==6)fixture_binding.metadata["type"]="different";
        if(fixture_fault==7)change_world_on_lookup=true;
        if(fixture_fault==8)host::batch_final_failure="original loaded Building final World proof failed";
        need(!resolve_identity_destination(object(102),fixture_loaded_target,error)&&!fixture_loaded_target.object&&!error.empty(),
             "loaded Building UID resolution lost pointer, retirement, ambiguity, metadata or complete World guard");
        host::batch_final_failure.clear();
    }
    canonical=setup(false);error.clear();
    need(!resolve_identity_destination(UINTPTR_MAX-0x10,fixture_loaded_target,error)&&!fixture_loaded_target.object&&
         error=="Service referenced destination pointer extent invalid","loaded Building accepted an overflowing pointer");
    error.clear();need(!resolve_identity_destination(object(999),fixture_loaded_target,error)&&!fixture_loaded_target.object&&
         error=="fixture target UID unreadable","loaded Building suppressed an unreadable UID failure");
    canonical=setup(true);lookup_override=object(102);LocalTarget target;error.clear();
    need(!resolve_identity_destination(object(101),target,error)&&!error.empty()&&!target.object,
         "referenced saved destination admitted a replaced full UID receiver");
    canonical=setup(true);host::batch_final_failure="original complete readonly final proof failed";error.clear();
    need(!identity_building_targets(owners,error,&destinations)&&error==host::batch_final_failure&&owners.empty()&&destinations.empty(),
         "Building-only census published before the complete final proof");
    error.clear();need(!resolve_identity_destination(object(101),target,error)&&error==host::batch_final_failure&&!target.object,
         "referenced destination published before complete final proof");host::batch_final_failure.clear();
    canonical=setup(false);std::vector<LocalTarget> full;
    need(identity_targets(full,error)&&full.size()==4&&saved_prop_reads==1&&settlement_reads[101]==2,
         "default Release census lost complete fresh Scenery proof");
    for(unsigned fault=0;fault<8;++fault){
        reset();fixture_modular=true;auto& value=live.at(101);value.type="Scenery";
        registry.at(registry_id(::uuid,key(101))).metadata["type"]="Scenery";
        if(fault==1)fixture_modular_art=true;
        if(fault==2)fixture_modular_layout=false;
        if(fault==3)fixture_modular_root=false;
        if(fault==4)fixture_modular_valid=false;
        if(fault==5)fixture_modular_memory=false;
        if(fault==6)registry.at(registry_id(::uuid,key(101))).revoked=true;
        if(fault==7)registry.at(registry_id(::uuid,key(101))).unknown=true;
        error.clear();const bool accepted=identity_building_targets(owners,error,&destinations);
        if(fault==4||fault==5||fault==7)need(!accepted&&!error.empty()&&owners.empty()&&destinations.empty(),"Modular census concealed original owner failure");
        else {
            need(accepted&&owners.size()==(fault?3u:4u),"Modular uptime inclusion broadened Art/non-root/non-Modular owners or omitted exact Modular");
            if(!fault)need(owners.front().uid==101&&!owners.front().normal_building&&!owners.front().normal_scenery&&fixture_modular_proofs==1,"Modular uptime changed normal Building/Service UI classification");
            else need(!fixture_modular_proofs,"excluded or revoked Modular entered original proof");
            ServiceLocalFixture local;local.service_destinations=destinations;ServiceBindingFixture binding;
            need(service_owners_actual(owners,local,binding,error)&&local.services.size()==owners.size(),"actual Service Frame loop omitted an admitted Modular uptime owner");
            if(!fault){
                need(local.services.front().key==key(101)&&local.services.front().uid==101,"actual Service Frame lost Modular full UID/key");
                local.services.clear();++binding.native_generation;
                need(!service_owners_actual(owners,local,binding,error)&&local.services.empty(),"actual Modular Frame loop bypassed current World binding");
            }
        }
        if(fault==4)need(error=="original Modular Layout/Plan proof failed","Modular census original cause lost");
        if(fault==5)need(error=="original Modular Art flag unreadable","Modular census read failure lost");
    }
    reset();
}
