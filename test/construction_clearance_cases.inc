void clearance_placement_cases(Runtime& runtime){
    using namespace fixture;std::string error;
    const auto setup=[&]{reset(runtime);actual_scenery=false;building={};building.type="SmallInn";building.variant="base";building.alive=true;
        request.building=building;clearance_placement_fixture::with_scenery=true;
        runtime.engine.world_.rows_[clearance_placement_fixture::scenery_key].state=clearance_placement_fixture::scenery;
    };
    setup();
    need(runtime.engine.perform_place(binding,request,nullptr,error)==action::Outcome::Confirmed&&places==1&&finishes==1&&
        runtime.engine.world_.rows_.at(request.canonical_record_key).state==building&&
        !runtime.engine.world_.rows_.at(clearance_placement_fixture::scenery_key).state.alive,
        "actual normal placement must publish Building birth and exact clearance together: "+error);
    need(clearance_placement_fixture::events==std::vector<std::string>{"capture","current","settle","finish","publication"}&&
        runtime.engine.buildings_.at(request.action_uuid).construction_confirmed,
        "actual Place bypassed a clearance phase or construction confirmation");
    for(const std::string phase:{"capture","current","settle","finish","publication"}){
        setup();clearance_placement_fixture::failure=phase;
        const auto result=runtime.engine.perform_place(binding,request,nullptr,error);
        const bool before_original=phase=="capture"||phase=="current";
        need(result==(before_original?action::Outcome::RejectedNoEffect:action::Outcome::Unknown)&&places==(before_original?0u:1u)&&
            error=="original placement clearance "+phase+" failure"&&runtime.engine.world_.rows_.size()==1&&
            runtime.engine.world_.rows_.at(clearance_placement_fixture::scenery_key).state.alive,
            "clearance phase failure lost cause/effect distinction or partially published World: "+error);
    }
    setup();commit_ok=false;
    need(runtime.engine.perform_place(binding,request,nullptr,error)==action::Outcome::Unknown&&places==1&&
        runtime.engine.world_.rows_.size()==1&&runtime.engine.world_.rows_.at(clearance_placement_fixture::scenery_key).state.alive&&
        !runtime.engine.buildings_.at(request.action_uuid).construction_confirmed,
        "failed atomic World commit published clearance/Building or confirmed a partial Job");
    reset(runtime);
}
