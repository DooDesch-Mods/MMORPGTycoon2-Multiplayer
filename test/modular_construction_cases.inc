void modular_construction_cases(Runtime& runtime){using namespace fixture;std::string error;std::vector<developer_staff::construction::LocalJob> jobs;
    const auto setup=[&]{reset(runtime);duplicate_engine_fixture::clear();building={};building.type="Scenery";building.variant="base";building.alive=true;
        request.action=action::Action::EditModularBuilding;request.modular.mode=0;request.canonical_record_key="modular:"+request.action_uuid;error.clear();};
    setup();need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed,error);
    need(construction_jobs(context,binding,jobs,error)&&jobs.size()==1&&jobs[0].building_key=="action:"+request.action_uuid&&jobs[0].building_uid==500&&jobs[0].job==0x1234,error);
    need(calls==std::vector<std::string>{"capture","modular-original","resolve","finish","modular-commit"},"Modular Job census must bracket original creation and precede publication");
    need(runtime.engine.perform_modular(binding,request,nullptr,error)!=action::Outcome::Confirmed&&duplicate_engine_fixture::native_calls==1,"Modular birth replayed its original allocator");
    auto undo=request;undo.action=action::Action::UndoBuilding;undo.target_action_uuid=request.action_uuid;undo.action_uuid="33333333-3333-4333-8333-333333333333";
    need(runtime.engine.perform_modular(binding,undo,nullptr,error)==action::Outcome::Confirmed&&construction_jobs(context,binding,jobs,error)&&!jobs[0].target_alive&&!jobs[0].target_restored,error);
    auto redo=undo;redo.action=action::Action::RedoBuilding;redo.action_uuid="44444444-4444-4444-8444-444444444444";
    need(runtime.engine.perform_modular(binding,redo,nullptr,error)==action::Outcome::Confirmed&&construction_jobs(context,binding,jobs,error)&&jobs[0].target_alive&&jobs[0].target_restored&&jobs[0].job==0x1234,error);
    need(finishes==1&&captures==1,"Modular history recreated original construction occurrence");
    for(int fault=0;fault<4;++fault){setup();need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed,error);
        auto& op=runtime.engine.modular_construction_.at(request.action_uuid);if(fault==0)++op.binding.native_epoch;if(fault==1)op.request.snapshot_uuid="foreign";if(fault==2)op.request.modular.mode=1;if(fault==3)++op.jobs[0].building_uid;
        need(runtime.engine.perform_modular(binding,undo,nullptr,error)==action::Outcome::RejectedNoEffect&&duplicate_engine_fixture::native_calls==1,"Modular history executed against foreign original construction source");}
    setup();need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed,error);need(runtime.engine.perform_modular(binding,undo,nullptr,error)==action::Outcome::Confirmed,error);commit_ok=false;
    need(runtime.engine.perform_modular(binding,redo,nullptr,error)==action::Outcome::Unknown&&!runtime.engine.modular_construction_.at(request.action_uuid).restored,"Failed Modular Redo publication marked construction restored");
    for(int fault=0;fault<3;++fault){setup();if(fault==0)capture_ok=false;if(fault==1)finish_ok=false;if(fault==2)commit_ok=false;
        need(runtime.engine.perform_modular(binding,request,nullptr,error)==(fault==0?action::Outcome::RejectedNoEffect:action::Outcome::Unknown)&&runtime.engine.world_.rows_.empty(),"Modular failure published partial World");
        if(fault==2)need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"Unconfirmed Modular Job escaped failed World publication");}
    for(int fault=0;fault<5;++fault){setup();need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed,error);
        auto& op=runtime.engine.modular_construction_.at(request.action_uuid);if(fault==0)++op.binding.native_epoch;if(fault==1)op.request.snapshot_uuid="foreign";if(fault==2)op.request.modular.mode=1;if(fault==3)op.request.action_uuid="foreign";if(fault==4)op.jobs[0].building_key="action:foreign";
        need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"Modular construction accepted foreign occurrence ownership");}
    setup();need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed,error);auto& old=runtime.engine.modular_construction_.at(request.action_uuid).jobs[0];expected_retired_before_place=&old.retired;
    request.action_uuid="55555555-5555-4555-8555-555555555555";request.canonical_record_key="modular:"+request.action_uuid;duplicate_engine_fixture::create_revision=3;
    need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed&&old.retired,error);expected_retired_before_place=nullptr;
    need(construction_jobs(context,binding,jobs,error)&&jobs.size()==2&&jobs[0].job==jobs[1].job&&jobs[0].retired&&!jobs[1].retired,"Modular allocator reuse merged original occurrences");
    setup();no_job=true;need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed&&construction_jobs(context,binding,jobs,error)&&jobs.empty(),"Modular native no-Job placement invented occurrence");
    setup();request.modular.mode=1;need(runtime.engine.perform_modular(binding,request,nullptr,error)==action::Outcome::Confirmed&&captures==0&&finishes==0&&runtime.engine.modular_construction_.empty(),"Modular existing-layout edit entered Prefab construction path");
}
