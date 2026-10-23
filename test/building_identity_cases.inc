#include "building_identity_index_baseline.inc"
#include "building_identity_index_pre_arena.inc"
#include "building_identity_index_measured.inc"
#include "building_targets_arena_baseline.inc"
#include "building_targets_arena_measured.inc"
#include "building_identity_index_ordered_arena.inc"
#include "building_targets_ordered_arena.inc"
class BuildingIndexAllocations final:public std::pmr::memory_resource {
    std::pmr::memory_resource* previous_;
    void* do_allocate(size_t bytes,size_t alignment)override{++calls;total_bytes+=bytes;return std::pmr::new_delete_resource()->allocate(bytes,alignment);}
    void do_deallocate(void* pointer,size_t bytes,size_t alignment)override{std::pmr::new_delete_resource()->deallocate(pointer,bytes,alignment);}
    bool do_is_equal(const std::pmr::memory_resource& other)const noexcept override{return this==&other;}
public:
    size_t calls=0,total_bytes=0;
    BuildingIndexAllocations():previous_(std::pmr::set_default_resource(this)){}
    ~BuildingIndexAllocations(){std::pmr::set_default_resource(previous_);}
};
size_t candidate_key_bytes=0,candidate_key_calls=0;
const std::string& candidate_key_probe(const std::string& key){candidate_key_bytes+=key.size();++candidate_key_calls;return key;}
#include "building_targets_baseline.inc"
#include "building_targets_tracked.inc"
void building_target_destination_copy_checks(){
    const auto setup=[](int scenario){
        host::batch_final_failure.clear();
        const auto canonical=natural_setup();auto& saved=registry.at(registry_id(::uuid,canonical));
        saved.scene_manager=0x71000000;saved.scene_region=fixture_saved_region;saved.scene_shard=fixture_root;
        if(scenario==1||scenario==2){for(uint64_t uid=102;uid<=104;++uid){registry.erase(registry_id(::uuid,key(uid)));live.erase(uid);}}
        if(scenario==2){const auto prototype=saved;registry.clear();
            for(uint64_t n=0;n<1528;++n){auto row=prototype;row.uid=n+5000;row.entry=n+1;row.object=object(row.uid);
                row.key=row.sha+":world:scenery:inline:"+std::to_string(n);row.scene_record=row.key;
                registry.emplace(registry_id(::uuid,row.key),std::move(row));}}
        if(scenario==3)saved.unknown=true;
        if(scenario==4){auto copy=saved;copy.key+="duplicate";registry.emplace(registry_id(::uuid,copy.key),copy);}
        if(scenario==5){auto copy=saved;copy.key+="duplicate";copy.uid=999;registry.emplace(registry_id(::uuid,copy.key),copy);}
        if(scenario==6){auto node=registry.extract(registry_id(::uuid,canonical));node.key()+="invalid";registry.insert(std::move(node));}
        if(scenario==7)world_failure=true;
        if(scenario==8)heap_failure=true;
        if(scenario==9)lookup_failure=true;
        if(scenario==10)identity_failure=104;
        if(scenario==11)scene_scenery::append_fixture=[](json&,std::string& cause){cause="original pending scene admission failed";return false;};
        if(scenario==12)scene_scenery::append_fixture=[](json&,std::string&){registry.begin()->second.scene_manager=0;return true;};
        if(scenario==13)host::batch_final_failure="original complete readonly final proof failed";
        if(scenario==14)saved.metadata.erase("type");
        if(scenario==15)saved.revoked=saved.unknown=true;
        if(scenario==16)live.at(104).prop.y=12;
        // An omitted pointer can collide with a freshly resolved Building whose
        // admitted binding had no pointer hint. Keep the closing collision guard.
        if(scenario==17)saved.object=object(102);
        if(scenario==18){world_failure=true;saved.unknown=true;}
    };
    const auto run=[&](auto function,int scenario,bool with_destinations){
        setup(scenario);candidate_key_bytes=candidate_key_calls=0;
        std::vector<LocalTarget> owners(1);std::map<uintptr_t,std::string> destinations{{1,"old output"}};
        std::string error="retained caller cause",exception;bool accepted=false;
        try{accepted=function(owners,error,with_destinations?&destinations:nullptr);}
        catch(const std::exception& failure){exception=failure.what();}
        json projected=json::array();for(const auto& row:owners)projected.push_back({
            row.manager,row.object,row.uid,row.world_epoch,row.world_generation,row.snapshot_id,row.record_key,
            row.type,row.variant,row.normal_building,row.normal_scenery,row.kind_cause});
        auto result=std::tuple{accepted,error,exception,projected,destinations,registry,next_entry,registry_generation,
            admitted_world,live_world,settlement_reads,refund_calls,registered_reads,prop_reads,world_reads,logs,seals,
            refund_uids,saved_prop_reads,fixture_modular_proofs,scene_scenery::append_calls};
        return std::tuple{std::move(result),candidate_key_calls,candidate_key_bytes};
    };
    for(bool with_destinations:{false,true})for(int scenario=0;scenario<19;++scenario){
        const auto baseline=run(identity_building_targets_baseline,scenario,with_destinations);
        const auto actual=run(identity_building_targets,scenario,with_destinations);
        const auto arena_baseline=run(identity_building_targets_arena_baseline,scenario,with_destinations);
        const auto arena_measured=run(identity_building_targets_arena_measured,scenario,with_destinations);
        const auto ordered_arena=run(identity_building_targets_ordered_arena,scenario,with_destinations);
        const auto tracked=run(identity_building_targets_tracked,scenario,with_destinations);
        need(std::get<0>(baseline)==std::get<0>(actual)&&std::get<0>(actual)==std::get<0>(tracked),
             "omitted destination optimization changed full outputs, error order, registry, native-read counts or original exception");
        need(std::get<0>(arena_baseline)==std::get<0>(actual)&&std::get<0>(arena_measured)==std::get<0>(actual),
             "local arena or moved exclusions changed complete production results, failure order, reads or side effects");
        need(std::get<0>(ordered_arena)==std::get<0>(actual),
             "unordered indexes changed full outputs, failure order, native reads or registry state");
        need(std::get<1>(baseline)==std::get<1>(tracked),"omitted destination optimization skipped a pointer collision insertion");
        if(with_destinations)need(std::get<2>(baseline)==std::get<2>(tracked),"destination caller lost canonical key bytes");
        else need(std::get<2>(tracked)==0,"Thought caller still copied an unused omitted destination key");
        if(scenario==2){
            need(std::get<1>(baseline)==1528&&std::get<2>(baseline)>1528*64,"large omitted census did not exercise every long canonical key");
            need(std::get<0>(std::get<0>(actual)),"complete large saved Scenery census rejected");
        }
        if(scenario==17)need(!std::get<0>(std::get<0>(actual))&&std::get<1>(std::get<0>(actual))=="Building owner census current receiver is ambiguous",
             "omitted pointer collision with fresh Building was not rejected at its original guard");
    }
    const auto measure=[&](auto function,bool destinations){
        setup(2);BuildingIndexAllocations allocations;std::vector<LocalTarget> owners;std::map<uintptr_t,std::string> keys;std::string error;
        const auto start=std::chrono::steady_clock::now();
        for(int repetition=0;repetition<20;++repetition)need(function(owners,error,destinations?&keys:nullptr),"large index allocation measurement rejected");
        const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/20;
        return std::tuple{allocations.calls/20,allocations.total_bytes/20,elapsed};
    };
    for(bool destinations:{false,true}){
        const auto prior=measure(identity_building_targets_arena_measured,destinations);
        const auto ordered=measure(identity_building_targets_ordered_arena,destinations);
        const auto current=measure(identity_building_targets,destinations);
        need(std::get<0>(prior)>=1528*4&&std::get<0>(current)<std::get<0>(prior)/20,
             "local arena did not remove per-node upstream allocations from both fresh UID/object indexes");
        std::cout<<"Building index 1528 rows destinations="<<destinations<<" allocations="<<std::get<0>(prior)<<"->"<<std::get<0>(current)
                 <<" bytes="<<std::get<1>(prior)<<"->"<<std::get<1>(current)<<" mean_ms="<<std::get<2>(prior)<<"->"<<std::get<2>(current)<<'\n';
        std::cout<<"Building unordered index 1528 rows destinations="<<destinations<<" allocations="<<std::get<0>(ordered)<<"->"<<std::get<0>(current)
                 <<" bytes="<<std::get<1>(ordered)<<"->"<<std::get<1>(current)<<" mean_ms="<<std::get<2>(ordered)<<"->"<<std::get<2>(current)<<'\n';
    }
    host::batch_final_failure.clear();reset();
}
void building_identity_index_checks(){
    const auto setup=[](){
        const auto canonical=natural_setup();auto& saved=registry.at(registry_id(::uuid,canonical));
        saved.scene_manager=0x71000000;saved.scene_region=fixture_saved_region;saved.scene_shard=fixture_root;
    };
    const auto compare=[&](bool accepted){
        host::WorldState world;world.origin=live_world;
        std::map<uint64_t,const Bound*> before;std::pmr::map<uint64_t,const Bound*> after;std::set<uint64_t> old_omitted,new_omitted;
        std::string old_error="retained caller cause",new_error=old_error;
        const bool prior=building_identity_index_baseline(world,fixture_image,before,old_omitted,old_error);
        const bool current=building_identity_index(world,fixture_image,after,new_omitted,new_error);
        need(prior==accepted&&current==prior&&old_error==new_error&&before==std::map<uint64_t,const Bound*>(after.begin(),after.end())&&old_omitted==new_omitted,
             "allocation-free Building index changed admission, original cause or complete/partial outputs");
        std::pmr::unordered_map<uint64_t,const Bound*> unordered;unordered.reserve(registry.size());
        std::set<uint64_t> unordered_omitted;std::string unordered_error="retained caller cause";
        const bool unordered_current=building_identity_index(world,fixture_image,unordered,unordered_omitted,unordered_error);
        need(unordered_current==current&&unordered_error==new_error&&before==std::map<uint64_t,const Bound*>(unordered.begin(),unordered.end())&&unordered_omitted==new_omitted,
             "unordered index changed admission, first failure or complete/partial UID and exclusion outputs");
        return new_omitted;
    };
    setup();need(compare(true)==std::set<uint64_t>{101},"Building index baseline omitted census differs");
    const auto prototype=std::find_if(registry.begin(),registry.end(),[](const auto& item){return item.second.uid==101;})->second;
    setup();registry.clear();
    for(uint64_t n=0;n<1528;++n){auto row=prototype;row.uid=n+5000;row.entry=n+1;row.object=object(row.uid);
        row.scene_manager=0x71000000;row.scene_region=fixture_saved_region;row.scene_shard=fixture_root;
        row.key="saved:scenery:"+std::to_string(n);row.scene_record=row.sha+":world:scenery:inline:"+std::to_string(n);
        registry.emplace(registry_id(::uuid,row.key),std::move(row));}
    need(compare(true).size()==1528,"large saved Scenery index lost exact exclusions");
    auto& fresh=registry.begin()->second;fresh.scene_manager=0;
    need(compare(true).size()==1527,"next complete index scan reused stale exclusion fields");
    fresh.unknown=true;compare(false);
    for(int fault=0;fault<13;++fault){
        setup();auto& row=registry.begin()->second;
        if(fault==0)unknown_locked=true;
        if(fault==1)++admitted_world.epoch;
        if(fault==2)row.unknown=true;
        if(fault==3)++row.world.epoch;
        if(fault==4)row.uid=0;
        if(fault==5)row.entry=0;
        if(fault==6)row.key.clear();
        if(fault==7)row.key+="suffix";
        if(fault==8){auto duplicate=row;duplicate.key+="duplicate";registry.emplace(registry_id(::uuid,duplicate.key),duplicate);}
        if(fault==9){row.object=object(999);auto duplicate=row;duplicate.key+="duplicate";duplicate.uid=999;
            registry.emplace(registry_id(::uuid,duplicate.key),duplicate);}
        if(fault>=10){auto node=registry.extract(registry.begin());
            node.key()=fault==10?"":fault==11?"1:wrong-snapshot:"+node.mapped().key:registry_id(::uuid,node.mapped().key)+"tail";
            registry.insert(std::move(node));}
        compare(false);
    }
    for(int skipped=0;skipped<4;++skipped){setup();auto& row=registry.begin()->second;row.unknown=true;
        if(skipped==0)++row.generation;if(skipped==1)row.snapshot="foreign";if(skipped==2)row.revoked=true;if(skipped==3)row.scene_owner="retired";
        compare(true);
    }
    for(int missing=0;missing<10;++missing){setup();auto found=std::find_if(registry.begin(),registry.end(),[](const auto& item){return item.second.uid==101;});auto& row=found->second;
        if(missing==0)row.object=0;if(missing==1)row.scene_owner="new";if(missing==2)row.scene_manager=0;
        if(missing==3)row.scene_region=0;if(missing==4)++row.scene_shard;if(missing==5)++row.class_addresspoint;
        if(missing==6)row.scene_record=row.sha+":world:scenery:inline";
        if(missing==7)row.scene_record=row.sha+"x:world:scenery:inline:";
        if(missing==8)row.scene_record=row.sha.substr(1)+":world:scenery:inline:";
        if(missing==9)row.scene_record="";
        need(compare(true).empty(),"incomplete original saved Scenery exclusion became eligible");
    }
    for(const auto& key:std::vector<std::string>{"a:b:c",std::string("key\0suffix",10),std::string(4096,'x')}){
        setup();auto node=registry.extract(registry.begin());node.mapped().key=key;node.key()=registry_id(::uuid,key);registry.insert(std::move(node));compare(true);
    }
    setup();auto& empty_sha=std::find_if(registry.begin(),registry.end(),[](const auto& item){return item.second.uid==101;})->second;empty_sha.sha.clear();
    empty_sha.scene_manager=0x71000000;empty_sha.scene_region=fixture_saved_region;empty_sha.scene_shard=fixture_root;
    empty_sha.scene_record=":world:scenery:inline:";need(compare(true)==std::set<uint64_t>{101},"empty SHA prefix changed original exclusion semantics");
    reset();
}
void building_identity_checks(){
    building_target_destination_copy_checks();
    building_identity_index_checks();
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
