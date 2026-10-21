void loaded_engine_cases(){
    using namespace fixture;Runtime runtime;reset(runtime);std::string error;std::vector<developer_staff::construction::LocalJob> jobs;
    const auto saved_key=std::string(64,'a')+":world:building:inline:region{60}/buildingManager/building[0]";
    auto& engine=runtime.engine;engine.loaded_construction_binding_=binding;engine.loaded_construction_snapshot_=context.snapshot_uuid;
    engine.loaded_construction_={{saved_key,0x2345,700,true,false,0x3456,0x4567,0x5678}};
    engine.world_.rows_[saved_key].state.alive=true;
    need(construction_jobs(context,binding,jobs,error)&&jobs.size()==1&&jobs[0].building_key==saved_key&&jobs[0].job==0x2345,
         "snapshot occurrence absent from construction projection");
    engine.world_.rows_[saved_key].state.alive=false;
    need(construction_jobs(context,binding,jobs,error)&&jobs.size()==1&&!jobs[0].target_alive,"loaded removed target discarded its original Job");
    engine.world_.rows_[saved_key].state.alive=true;
    NativeEngine::Operation removal;removal.request.action=action::Action::RemoveSnapshotBuilding;removal.request.action_uuid="remove";
    removal.request.snapshot_uuid=context.snapshot_uuid;removal.request.canonical_record_key=saved_key;removal.binding=binding;removal.local_uid=700;removal.history_ready=true;
    engine.operations_["remove"]=removal;
    NativeEngine::HistoryOperation restore;restore.request.action=action::Action::UndoBuilding;restore.request.action_uuid="restore";
    restore.request.target_action_uuid="remove";restore.request.snapshot_uuid=context.snapshot_uuid;restore.request.canonical_record_key=saved_key;
    restore.binding=binding;restore.uid=700;restore.confirmed=true;restore.object_before.alive=false;restore.object_after.alive=true;
    engine.history_["restore"]=restore;
    need(construction_jobs(context,binding,jobs,error)&&jobs[0].target_restored,"loaded snapshot Remove Undo did not preserve separate restored target lifetime");
    engine.history_["restore"].confirmed=false;
    need(construction_jobs(context,binding,jobs,error)&&!jobs[0].target_restored,"unconfirmed snapshot Remove Undo claimed restoration");
    engine.loaded_construction_binding_.native_generation++;
    need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"loaded occurrence crossed a native generation");engine.loaded_construction_binding_=binding;
    engine.loaded_construction_snapshot_="foreign";
    need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"loaded occurrence crossed a shared snapshot");engine.loaded_construction_snapshot_=context.snapshot_uuid;
    engine.world_.rows_.clear();need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"loaded occurrence missing world row admitted");engine.world_.rows_[saved_key].state.alive=true;
    engine.loaded_construction_.push_back(engine.loaded_construction_[0]);
    need(!construction_jobs(context,binding,jobs,error)&&jobs.empty(),"duplicate loaded canonical occurrence admitted");engine.loaded_construction_.pop_back();
    expected_retired_before_place=&engine.loaded_construction_[0].retired;
    need(engine.perform_place(binding,request,nullptr,error)==action::Outcome::Confirmed&&engine.loaded_construction_[0].retired,
         "absent loaded allocation not retired before later original placement reuse");
    expected_retired_before_place=nullptr;
    need(construction_jobs(context,binding,jobs,error)&&jobs.size()==2&&jobs[0].retired&&jobs[1].building_key==request.canonical_record_key,
         "loaded and later ordered placement occurrences were not retained together");
}
