// Actual phase orchestration and History admission; native scope leaves are
// mocked here. The batch fixture separately executes actual scope lifecycle.
void quest_phase_cases(){
    NativePort port;active=&port;std::string error;
    for(float before_pause:{0.0f,1.0f}){
        auto before=setup();before.history.pin.pause=before_pause;port.history_.expected=before.history;
        put(before.history.pin.clock,0x5c,before_pause);
        quest_state::Toon old,target;target.gold=1;
        before.state.subscribers[0].quest_state=old;
        before.state.subscribers[1].quest_state=old;
        State after=before.state;after.subscribers[0].quest_state=target;after.subscribers[1].quest_state=target;
        need(port.begin_quest_stores(before,error),"phase begin");phase_simulate=true;
        const auto body=[&](std::string& e){
            need(port.quest_batch_!=nullptr,"eligible phase owns lazy batch");
            port.quest_batch_->held=true;++phase_enters;put(before.history.pin.clock,0x5c,0.0f);
            need(port.admit_quest_history(before,e)&&port.quest_history_.pin.pause==0,"held full History admits closed pause0 or pause1 without altering actual Pin");
            auto changed=before;++changed.history.pin.common_epoch;
            need(!port.admit_quest_history(changed,e),"held History rejects another Pin field drift");
            changed=before;changed.history.pin.pause=2;
            need(!port.admit_quest_history(changed,e),"held History rejects unsupported raw pause transition");
            changed=before;changed.history.records.push_back(9);
            need(!port.admit_quest_history(changed,e),"held History retains complete record values");
            e.clear();
            put(uintptr_t(50000),0x850,int32_t(1));put(uintptr_t(60000),0x850,int32_t(1));
            return true;
        };
        need(port.with_quest_stores(before,after,body,error),"explicit phase closes and reads all changed targets");
        need(phase_enters==1&&phase_leaves==1&&!port.quest_batch_&&reads==2,"one phase close precedes two complete closed target reads");
        float actual_pause=-1;need(at(before.history.pin.clock,0x5c,actual_pause,error)&&actual_pause==before_pause,"close restores exact original pause state");
        port.end_quest_stores();
    }
    auto before=setup();quest_state::Toon old,target;target.gold=1;
    before.state.subscribers[0].quest_state=old;State after=before.state;after.subscribers[0].quest_state=target;
    need(port.begin_quest_stores(before,error),"no-effect phase begin");phase_simulate=true;
    need(port.with_quest_stores(before,after,[](std::string&){return true;},error)&&!phase_enters&&!phase_leaves,"lazy phase with no original helper performs no enter");
    port.end_quest_stores();
    // The actual store bodies execute two ordered Toons. Native Progress and
    // weapon leaves are mocked; the batch fixture covers original scope calls.
    for(int kind=0;kind<4;++kind){
        before=setup();quest_state::Toon first_old,first_target,second_old,second_target;
        quest_state::Progress prior;prior.npc={quest_state::LinkState::Live,"npc",-1};first_old.progress.push_back(prior);
        first_target.gold=1;second_target.gold=2;second_target.weapon_ilvl=2;
        init_toon(50000,first_old);
        before.state.subscribers[0].quest_state=first_old;
        before.state.subscribers[1].quest_state=kind==1?std::nullopt:std::optional<quest_state::Toon>(second_old);
        after=before.state;after.subscribers[0].quest_state=first_target;
        if(kind==1)second_target=second_old;
        after.subscribers[1].quest_state=kind==2?std::nullopt:std::optional<quest_state::Toon>(second_target);
        need(port.begin_quest_stores(before,error),"mixed phase begin");phase_simulate=true;phase_weapon_queues_tasks=kind==3;
        const auto rows=[&](std::string& e){
            need(port.quest_batch_!=nullptr,"weapon/lifecycle tuple no longer disables phase dispatch");
            if(!port.store_quest_state(before,0,first_old,first_target,e))return false;
            need(port.quest_batch_->held&&phase_enters==1&&retires==1,"first actual Toon retirement opens one lazy held scope");
            return port.store_quest_state(before,1,before.state.subscribers[1].quest_state,after.subscribers[1].quest_state,e);
        };
        const bool okay=port.with_quest_stores(before,after,rows,error);
        if(kind<2){
            need(okay&&phase_enters==1&&phase_leaves==1&&!port.quest_batch_,"two actual Toon stores share one explicit close with weapon/lifecycle change");
            need(callbacks==(kind==0?4u:3u),"original Progress/weapon/refresh/pause callback counts retained");
            need(toons[60000].weapon_ilvl==second_target.weapon_ilvl,"actual second-row weapon target retained through closed readback");
        }else if(kind==2){
            need(!okay&&phase_enters==1&&phase_leaves==1&&!port.quest_batch_&&error=="Quest apply requires actual Toon lifecycle transition before state stores","missing target retains original lifecycle rejection and guarded cleanup");
        }else{
            need(!okay&&phase_enters==1&&!phase_leaves&&!port.quest_batch_&&error=="Subscriber append original TaskManager not drained","queued task proof failure prevents unsafe phase close");
        }
        port.end_quest_stores();
    }
    for(int fault=0;fault<4;++fault){
        before=setup();target=old;target.gold=1;before.state.subscribers[0].quest_state=old;after=before.state;after.subscribers[0].quest_state=target;
        need(port.begin_quest_stores(before,error),"failure phase begin");phase_simulate=true;
        phase_history_on_exit=fault==0;phase_close_failure=fault==1;
        const auto body=[&](std::string& e){
            port.quest_batch_->held=true;++phase_enters;put(before.history.pin.clock,0x5c,0.0f);
            if(fault==2)port.fence_.okay=false;
            if(fault==3)return fail(e,"original row failure");
            return true;
        };
        need(!port.with_quest_stores(before,after,body,error)&&!port.quest_batch_,"history/fence/leave/body failure prevents phase success");
        need(reads==0,"failed close performs no closed target read or NPC fallthrough");
        if(fault==0)need(error=="Quest phase exit complete History/finance receipt differs"&&!phase_leaves,"exit after pointer clear still checks bound complete History");
        if(fault==1)need(error=="original phase leave failure"&&phase_leaves==1,"leave cause retained without retry");
        if(fault==2)need(error=="original History fence failure"&&!phase_leaves,"exit still checks bound economic fence");
        if(fault==3)need(error=="original row failure"&&phase_leaves==1,"row failure retains safe cleanup and original cause");
        port.end_quest_stores();
    }
    active=nullptr;
}
