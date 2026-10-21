void duplicate_engine_cases(Runtime& runtime){
    using namespace fixture;std::string error;std::vector<developer_staff::construction::LocalJob> jobs;
    const std::string duplicate_id="33333333-3333-4333-8333-333333333333";
    const std::string created_key="action:44444444-4444-4444-8444-444444444444";
    const auto setup=[&]{reset(runtime);duplicate_engine_fixture::clear();error.clear();
        building={};building.type="SmallInn";building.variant="base";building.alive=true;
        request.action=action::Action::DuplicateSelection;request.action_uuid=duplicate_id;request.canonical_record_key="duplicate:"+duplicate_id;
        action::DuplicateTarget row;row.key="action:55555555-5555-4555-8555-555555555555";row.record_sha=std::string(64,'a');
        row.family=action::DuplicateFamily::Building;row.region=60;row.cost=400;
        row.before[3]=row.after[3]=std::bit_cast<uint32_t>(1.0f);
        for(size_t i=7;i<10;++i)row.before[i]=row.after[i]=std::bit_cast<uint32_t>(1.0f);
        row.created_keys={created_key};request.duplicate.rows={row};duplicate_engine_fixture::targets={{500,created_key}};
    };
    setup();
    need(runtime.engine.perform_duplicate(binding,request,nullptr,error)==action::Outcome::Confirmed,
        "actual Engine Duplicate construction admission failed: "+error);
    need(duplicate_engine_fixture::native_calls==1&&duplicate_engine_fixture::finished==1&&
        construction_jobs(context,binding,jobs,error)&&jobs.size()==1&&jobs[0].building_key==created_key&&jobs[0].building_uid==500,
        "confirmed original Duplicate Job was omitted from complete construction projection: "+error);
    need(calls==std::vector<std::string>{"capture","duplicate-original","resolve","duplicate-finish","duplicate-commit"},
        "Duplicate queue proof must bracket original and finish before World publication");
    const auto birth=request;auto undo=birth;undo.action=action::Action::UndoBuilding;undo.target_action_uuid=duplicate_id;
    undo.action_uuid="66666666-6666-4666-8666-666666666666";
    need(runtime.engine.perform_duplicate_history(binding,undo,nullptr,error)==action::Outcome::Confirmed&&
        construction_jobs(context,binding,jobs,error)&&!jobs[0].target_alive&&!jobs[0].target_restored,
        "Duplicate Undo must retain expired exact original Job and tombstone: "+error);
    auto redo=undo;redo.action=action::Action::RedoBuilding;redo.action_uuid="77777777-7777-4777-8777-777777777777";
    need(runtime.engine.perform_duplicate_history(binding,redo,nullptr,error)==action::Outcome::Confirmed&&
        construction_jobs(context,binding,jobs,error)&&jobs[0].target_alive&&jobs[0].target_restored&&jobs[0].job==0x5678,
        "Duplicate Redo must mark restored target separately from original expired weak Job: "+error);
    need(duplicate_engine_fixture::native_calls==1&&duplicate_engine_fixture::history_calls==2,"history replayed original Duplicate creation");
    for(int wrong=0;wrong<3;++wrong){setup();
        if(wrong==0)capture_ok=false;if(wrong==1)duplicate_engine_fixture::finish_ok=false;if(wrong==2)duplicate_engine_fixture::commit_ok=false;
        const auto outcome=runtime.engine.perform_duplicate(binding,request,nullptr,error);
        need(outcome==(wrong==0?action::Outcome::RejectedNoEffect:action::Outcome::Unknown)&&runtime.engine.world_.rows_.empty()&&
            duplicate_engine_fixture::native_calls==(wrong==0?0u:1u),"Duplicate failure published partial state or replayed: "+error);
        if(wrong==2)need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"unconfirmed Duplicate Job escaped publication failure");
    }
    setup();need(runtime.engine.perform_duplicate(binding,request,nullptr,error)==action::Outcome::Confirmed,"Duplicate setup failed");
    auto& retained=runtime.engine.duplicate_construction_.at(duplicate_id);retained.binding.native_epoch++;
    need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"foreign Duplicate construction binding was admitted");
    setup();need(runtime.engine.perform_duplicate(binding,request,nullptr,error)==action::Outcome::Confirmed,"Duplicate reuse setup failed");
    auto& old=runtime.engine.duplicate_construction_.at(duplicate_id).jobs[0];expected_retired_before_place=&old.retired;
    request.action_uuid="88888888-8888-4888-8888-888888888888";request.canonical_record_key="duplicate:"+request.action_uuid;
    request.duplicate.rows[0].created_keys={"action:99999999-9999-4999-8999-999999999999"};
    duplicate_engine_fixture::targets={{500,request.duplicate.rows[0].created_keys[0]}};
    duplicate_engine_fixture::create_revision=runtime.engine.world_.revision()+1;
    const auto reused=runtime.engine.perform_duplicate(binding,request,nullptr,error);
    need(reused==action::Outcome::Confirmed&&old.retired,
        "absent Duplicate Job allocation was not retired before later original creation: "+error);
    expected_retired_before_place=nullptr;
    setup();request.duplicate.rows[0].family=action::DuplicateFamily::Scenery;actual_scenery=true;
    need(runtime.engine.perform_duplicate(binding,request,nullptr,error)==action::Outcome::Confirmed&&captures==0&&duplicate_engine_fixture::finished==0&&
        construction_jobs(context,binding,jobs,error)&&jobs.empty(),"pure Scenery Duplicate invented a construction Job");
    actual_scenery=false;
}
