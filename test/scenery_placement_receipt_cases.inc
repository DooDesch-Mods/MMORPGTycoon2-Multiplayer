void scenery_receipt_cases(Runtime& runtime,std::string& error){
    const auto prepare=[&]{
        reset(runtime);scenery_receipt_fixture::reset();fixture::actual_scenery=true;fixture::no_job=true;error.clear();
        fixture::building={"cactus","p_ext_treecactus_01.vmb",100,10,200,{0,0,0,1},{1,1,1},true};
        fixture::request.building=fixture::building;fixture::request.sequence=1;
    };
    prepare();
    fixture::need(runtime.engine.perform_place(fixture::binding,fixture::request,nullptr,error)==action::Outcome::Confirmed,
        "actual Scenery producer-to-consumer placement failed: "+error);
    const auto& operation=runtime.engine.buildings_.at(fixture::request.action_uuid);
    const auto& stored=scenery_receipt_fixture::producer.receipts.at(fixture::request.action_uuid);
    fixture::need(operation.receipt.at("local_undo_graph").get<uint64_t>()==scenery_receipt_fixture::graph&&
        stored.at("local_undo_graph")==operation.receipt.at("local_undo_graph"),"validated original Scenery graph missing from returned or retained receipt");
    fixture::need(operation.receipt.at("private_stage")==19&&operation.construction_confirmed&&operation.normal_scenery&&
        scenery_receipt_fixture::remembered==1&&scenery_receipt_fixture::original_updates==1,
        "Scenery history/lifecycle not confirmed exactly once");
    fixture::need(runtime.engine.perform_place(fixture::binding,fixture::request,nullptr,error)==action::Outcome::Unknown&&
        scenery_receipt_fixture::remembered==1&&scenery_receipt_fixture::original_updates==1,"confirmed Scenery original replayed");
    prepare();auto authority=std::make_unique<action::Evidence>();authority->object_after=fixture::building;
    fixture::need(runtime.engine.perform_place(fixture::binding,fixture::request,authority.get(),error)==action::Outcome::Confirmed&&
        scenery_receipt_fixture::remembered==1&&scenery_receipt_fixture::original_updates==1,
        "actual Client Scenery producer/Host authority/history join failed: "+error);
    for(int failure=0;failure<3;++failure){
        prepare();
        if(failure==0)scenery_receipt_fixture::graph_locked=false;
        if(failure==1)scenery_receipt_fixture::cleanup_ok=false;
        if(failure==2)scenery_receipt_fixture::world_stable=false;
        const auto outcome=runtime.engine.perform_place(fixture::binding,fixture::request,nullptr,error);
        const auto cause=error;
        const auto& receipt=runtime.engine.buildings_.at(fixture::request.action_uuid).receipt;
        fixture::need(outcome==action::Outcome::Unknown&&scenery_receipt_fixture::original_updates==1&&
            scenery_receipt_fixture::remembered==0&&!receipt.contains("local_undo_graph")&&!receipt.value("confirmed",true),
            "failed Scenery original published a successful history graph");
        fixture::need(cause==(failure==0?"undo_commit_not_drained_locked":failure==1?"actual original Scenery cleanup failure":"scenery native world changed before cleanup"),
            "Scenery original failure cause changed: "+cause);
        fixture::need(scenery_receipt_fixture::errors==std::vector<std::string>{cause},"original Scenery failure log was lost");
        fixture::need(runtime.engine.perform_place(fixture::binding,fixture::request,nullptr,error)==action::Outcome::Unknown&&
            scenery_receipt_fixture::original_updates==1,"partial Scenery original replayed");
    }
    prepare();scenery_receipt_fixture::history_ok=false;
    fixture::need(runtime.engine.perform_place(fixture::binding,fixture::request,nullptr,error)==action::Outcome::Unknown&&
        error=="actual history registration failure"&&scenery_receipt_fixture::remembered==1&&fixture::finishes==0,
        "history registration failure became a confirmed Scenery action");
    fixture::need(runtime.engine.perform_place(fixture::binding,fixture::request,nullptr,error)==action::Outcome::Unknown&&
        scenery_receipt_fixture::original_updates==1&&scenery_receipt_fixture::remembered==1,"history failure replayed Scenery placement");
    fixture::actual_scenery=false;
}
