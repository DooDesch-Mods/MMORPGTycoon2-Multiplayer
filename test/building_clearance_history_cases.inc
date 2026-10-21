namespace clearance_history_cases {
namespace bc=ui_move_runtime::building_clearance;
void enable(Harness& harness){
    bc::fixture_events.clear();bc::fixture_failure.clear();
    bc::fixture_scenery_state={};bc::fixture_scenery_state.type="cactus";bc::fixture_scenery_state.variant="p_ext_treecactus_01.vmb";bc::fixture_scenery_state.alive=true;
    const auto origin=harness.world_.origin();const auto primary=harness.world_.rows().at(harness.key);std::string error;
    const auto scenery_record=mt2mp::game::scenery_scene_key::root_record(origin.sha256,67,3,0);
    harness.world_={};
    const bool bound=harness.world_.bind_snapshot(origin,{{harness.key,primary.snapshot_record_key,primary.state},
        {bc::fixture_scenery_key,scenery_record,bc::fixture_scenery_state}},1,error);
    check(bound,"Clearance history fixture snapshot bootstrap: "+error);
    check(harness.world_.retire_native_effects(origin,2,fixture_placed,{bc::fixture_scenery_key},error),error);
    harness.buildings_.at(fixture_placed).clearance_after.local=std::make_shared<bc::Local>();
}
void run(){
    std::string error;
    {Harness harness(false);world_state::Table invalid;
        const bool bound=invalid.bind_snapshot(harness.world_.origin(),
            {{harness.key,"",harness.world_.rows().at(harness.key).state}},1,error);
        check(!bound&&error=="world_snapshot_object_id_record_or_state_invalid"&&invalid.rows().empty()&&invalid.revision()==0,
            "Original empty saved-record fixture must fail before World publication: "+error);}
    {Harness harness(false);enable(harness);
        check(harness.run(error)==action::Outcome::Confirmed&&calls==1&&harness.world_.revision()==3&&
            !harness.world_.rows().at(harness.key).state.alive&&harness.world_.rows().at(bc::fixture_scenery_key).state.alive,
            "Actual placement Undo must atomically retire Building and restore clearance Scenery at one revision: "+error);
        fixture_revision=4;pending=redo_id;
        check(harness.run(error,nullptr,false)==action::Outcome::Confirmed&&calls==2&&harness.world_.revision()==4&&
            harness.world_.rows().at(harness.key).state.alive&&!harness.world_.rows().at(bc::fixture_scenery_key).state.alive,
            "Actual placement Redo must atomically restore Building and retire the same clearance set: "+error);
        check(bc::fixture_events==std::vector<std::string>{"prepare","settle","finish","publication","prepare","settle","finish","publication"},
            "Actual history lost clearance preflight/original settlement/Record/publication order");
        check(harness.run(error,nullptr,false)==action::Outcome::Unknown&&calls==2,"Clearance history replayed its original");}
    for(const std::string failure:{"prepare","settle","finish","publication"}){
        Harness harness(false);enable(harness);bc::fixture_failure=failure;
        const auto outcome=harness.run(error);
        check(outcome==(failure=="prepare"?action::Outcome::RejectedNoEffect:action::Outcome::Unknown)&&calls==(failure=="prepare"?0:1)&&
            error.find("original clearance")!=std::string::npos&&harness.world_.revision()==2,
            "Actual clearance history hid a failed phase or published a partial World table: "+error);
    }
    bc::fixture_failure.clear();
}
}
