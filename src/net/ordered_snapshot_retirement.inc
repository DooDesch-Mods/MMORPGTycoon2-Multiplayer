static json ordered_last_retirement=nullptr;
bool retire_ordered_snapshot(const game::ordered_session::ProofOwner* owner,std::string& error){
    try{
        if(!ordered_snapshot){error="Ordered snapshot retirement requires its retained selected owner";return false;}
        if(GetCurrentThreadId()!=ordered_thread){error="Ordered snapshot retirement requires its bound game thread";return false;}
        if(!ordered_title(error))return false;
        const auto& identity=*ordered_snapshot;
        game::typed_action_runtime::OrderedRetirementState runtime;
        if(!game::typed_action_runtime::read_ordered_retirement(runtime,error))return false;
        if(owner){
            if(!owner->retired_identity(identity.session,identity.snapshot_uuid,identity.native_generation,error))return false;
        }else if(runtime.present||ordered_step_service||ordered_load_requested){
            error="Ordered snapshot retirement cannot omit a registered proof owner";return false;
        }
        if(runtime.present){
            const auto& context=runtime.context;
            if(!owner||context.session!=identity.session||context.snapshot_uuid!=identity.snapshot_uuid||context.local_peer!=identity.local_peer||
               context.host_peer!=1||context.peer_mask!=identity.peer_mask||
               context.role!=(identity.local_peer==1?action_session::Role::Host:action_session::Role::Client)){
                error="Ordered Runtime retirement identity differs from retained physical snapshot owner";return false;
            }
        }else if(ordered_step_service||ordered_load_requested){
            error="Ordered snapshot retirement lost its registered Runtime before native callback acknowledgement";return false;
        }
        if(identity.native_prepared){
            const auto native=game::building_mixed_identity::opened_file::status();
            if(native.uuid!=identity.snapshot_uuid||native.generation!=identity.native_generation||native.game_thread!=ordered_thread){
                error="Ordered snapshot retirement NativeOpen generation/thread differs";return false;
            }
            if(native.native_request_attempted){
                if(native.held||!native.native_task_retired){error="Ordered snapshot retirement awaits actual NativeOpen task/file retirement";return false;}
            }else if(native.held&&!game::native_open_runtime::retire_unstarted(identity.native_generation,error))return false;
        }
        if(!outbound.retire(error)||!monster_outbound.retire(error)||!buffer.retire(error)||!monster_buffer.retire(error))return false;
        ordered_last_retirement={{"session",identity.session},{"snapshot_uuid",identity.snapshot_uuid},{"native_generation",identity.native_generation},
            {"failure",ordered_failure.empty()?snapshot_failure:ordered_failure},{"runtime_failure",runtime.failure}};
        game::normal_load_identity::cancel("ordered snapshot owner retired after actual callback acknowledgements and Title");
        receiving=load_pending=outbound_active=snapshot_failed=snapshot_world_valid=false;
        incoming_id.clear();incoming_hash.clear();incoming_world_name="MPTestSession";outbound_id.clear();last_loaded.clear();
        incoming_started={};incoming_load={};load_observed_at={};load_observation_sent=false;incoming_origin_generation=0;
        outbound_peers.clear();outbound_expected_peers=0;outbound_progress={};outbound_queued_chunks=0;
        incoming_monsters=outbound_monsters=last_loaded_ack=host_failure_notice=nullptr;
        incoming_network.reset();outbound_network.reset();outbound_network_receipt={};incoming_network_error.clear();
        incoming_software={};incoming_epoch=discarded_after_failure=0;snapshot_failure.clear();
        ordered_load_watch={};ordered_load_requested=ordered_load_complete=ordered_load_acknowledged=false;ordered_normal_receipt={};
        ordered_step_service.reset();ordered_step_lease=0;ordered_bootstrap_hashes={};ordered_bootstrap_ready_hash.clear();
        ordered_bootstrap_sent=ordered_bootstrap_ready=false;
        ordered_covered_hashes={};ordered_covered_ready_hash.clear();ordered_covered_evidence_path.clear();
        ordered_covered_attempted=false;ordered_covered_watch={};
        ordered_manifest=nullptr;ordered_source_path.clear();ordered_failure.clear();ordered_thread=0;ordered_snapshot.reset();ordered_original_cohort=false;
        status="Ordered snapshot owner retired";error.clear();return true;
    }catch(const std::exception& cause){error=cause.what();return false;}
}
