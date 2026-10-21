void observer_deadline_cases(){
    namespace d=mt2mp::game::placement_ui_diagnostics;using Json=nlohmann::json;
    const std::string action="11111111-1111-4111-8111-111111111111";
    d::Job j;j.reserved=true;j.armed_at=100;j.input.native.token=action;j.input.snapshot_uuid=action;
    j.input.epoch=3;j.input.generation=4;j.binding.behaviour=100;
    j.receipt.phase=ui::Phase::ArmedAwaitingGameUpdate;
    require(d::observation_timeout(j,10100).empty(),"capture deadline expired before its full bound");
    const auto original_timeout=d::observation_timeout(j,10101);
    require(!original_timeout.empty(),"missing original capture has no finite deadline");
    j.receipt.capture_count=1;j.receipt.action_uuid=action;j.receipt.phase=ui::Phase::CapturedAwaitingHost;
    require(d::observation_timeout(j,60000).empty(),"observer charged bounded Controller phases to the input timer");
    Json controller={{"started",true},{"action_uuid",action},{"outcome",0},{"pending",true},{"locked",false}};
    std::string error;
    require(d::observe_confirmation(j,controller,Json::object(),error)&&j.receipt.phase==ui::Phase::CapturedAwaitingHost,"exact pending Controller was falsely failed");
    auto failed=controller;failed["outcome"]=3;failed["cause"]="original native effect failed";
    auto bad=j;require(!d::observe_confirmation(bad,failed,Json::object(),error)&&bad.receipt.cause.find("original native effect failed")!=std::string::npos,"Controller failure hidden behind fresh-frame wait");
    controller["outcome"]=1;controller["pending"]=false;controller["required_mask"]=3;controller["ack_mask"]=3;
    Json native={{action,{{"confirmed",true},{"undo_confirmed",true},{"native_uid",77},{"purchase",1},{"add",1},{"undo",1},{"exception",0}}}};
    auto confirmed=j;
    require(d::observe_confirmation(confirmed,controller,native,error)&&confirmed.receipt.phase==ui::Phase::HostConfirmed,"exact retired native receipt and Host ACK proof rejected");
    j.confirmation_observed=true;j.confirmation_observed_at=60000;
    require(d::observation_timeout(j,70000).empty()&&!d::observation_timeout(j,70001).empty(),"fresh frame wait lacks its own finite ten-second bound");
    require(j.confirmation_observed_at==60000,"repeated fresh-frame observation reset its deadline");
    j.receipt.phase=ui::Phase::Unknown;j.receipt.cause=original_timeout;
    confirmed.fresh_pose={{"native_uid",77},{"type","SmallInn"},{"variant","base"},{"x",1.0},{"y",2.0},{"z",3.0},{"rotation",Json::array({0,0,0,1})},{"scale",Json::array({1,1,1})}};
    for(int wrong=0;wrong<5;++wrong){
        auto before=j,observed=confirmed;
        if(wrong==0)before.receipt.cause="original native callback failed";
        if(wrong==1)before.receipt.capture_count=0;
        if(wrong==2)observed.input.snapshot_uuid="foreign";
        if(wrong==3)observed.fresh_pose=nullptr;
        if(wrong==4)observed.receipt.action_uuid="foreign";
        const auto cause=before.receipt.cause;
        require(!d::publish_reconciliation(before,observed,error)&&before.receipt.phase==ui::Phase::Unknown&&before.receipt.cause==cause,"timeout reconciliation accepted foreign/incomplete proof or erased original failure");
    }
    require(d::publish_reconciliation(j,confirmed,error)&&j.receipt.phase==ui::Phase::HostConfirmed&&j.receipt.capture_count==1&&j.fresh_pose==confirmed.fresh_pose&&j.recovered_timeout_cause==original_timeout,"read-only timeout reconciliation lost original cause, full pose or exact-once capture");
    require(!d::can_reconcile_timeout(j),"confirmed readback reopened mutation or reconciliation");
}
