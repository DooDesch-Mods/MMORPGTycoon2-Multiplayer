void client_application_cases(std::chrono::steady_clock::time_point now){
    using Ready=action_session::AuthorityReadiness;
    std::string e;action_session::Message result;
    auto prime=[](Bridge& b){
        world_fixture={};world_fixture.context.role=action_session::Role::Client;world_fixture.context.local_peer=2;
        b.context_.role=action_session::Role::Client;b.context_.local_peer=2;b.port_.native.context_=world_fixture.context;
        b.queued_=Frame{};b.queued_->management=77;b.queued_day_=78;b.queued_economy_=79;
        admission_result=Admission::New;
    };
    auto retained=[](const Bridge& b){return b.queued_&&b.queued_->management==77&&b.queued_day_==78&&b.queued_economy_==79;};
    {Bridge b;prime(b);world_fixture.saves=1;
        for(int i=0;i<3;++i)check(b.tick(0,false,e,now)&&retained(b)&&b.takes==0&&!b.locked_&&!b.last_,"save wait retains same complete ordinary Frame before all take side effects");
        check(b.publication_wait_.find("Client Frame waits for native save saves=1 edits=0")!=std::string::npos,"ordinary wait exposes measured cause to bounded logger");
        check(b.application_waiting_&&b.application_wait_count_==3&&b.application_recovery_count_==0&&b.publication_wait_count_==0,"Client observations remain separate from Host publication counters");
        world_fixture.saves=0;
        check(b.tick(0,false,e,now)&&b.takes==1&&!b.queued_&&!b.queued_day_&&!b.queued_economy_&&b.publication_wait_.empty(),"idle applies retained ordinary Frame exactly once and clears all queued components");
        check(b.tick(0,false,e,now)&&b.takes==1,"following idle tick cannot replay consumed Frame");
        check(!b.application_waiting_&&b.application_recovery_count_==1&&!b.application_first_wait_.empty()&&b.application_first_wait_==b.application_last_wait_,"completed save retains first/last cause and exactly one recovery");
    }
    {Bridge b;prime(b);world_fixture.saves=2;
        for(int i=0;i<3;++i)check(b.before_ready(1,result,0,e)==Ready::Wait&&retained(b)&&b.takes==0,"HostResult readiness defers same full before Frame without take");
        world_fixture.saves=0;
        check(b.before_ready(1,result,0,e)==Ready::Ready&&retained(b)&&b.takes==0,"HostResult readiness recovery does not itself apply or consume");
        check(b.before_result(1,result,0,e)&&b.takes==1&&!b.queued_&&!b.queued_day_&&!b.queued_economy_,"HostResult applies same recovered Frame exactly once");
        check(!b.before_result(1,result,0,e)&&b.takes==1,"already consumed before Frame cannot replay");
    }
    {Bridge b;prime(b);
        check(b.before_ready(1,result,0,e)==Ready::Ready,"idle before Frame ready");world_fixture.saves=1;
        check(!b.before_result(1,result,0,e)&&retained(b)&&b.takes==0&&e.find("began after readiness; no replay")!=std::string::npos,"save beginning after committed readiness fails closed rather than pretending wait succeeded");
    }
    for(auto activity:{std::pair{1,1},std::pair{1,-1},std::pair{-1,0},std::pair{0,1}}){
        Bridge b;prime(b);world_fixture.saves=activity.first;world_fixture.edits=activity.second;
        check(!b.tick(0,false,e,now)&&retained(b)&&b.takes==0&&e.find("save/edit scope differs")!=std::string::npos,"non-save edit and malformed counter never become ordinary wait");
        check(b.before_ready(1,result,0,e)==Ready::Rejected&&retained(b)&&b.takes==0,"non-save edit and malformed counter reject HostResult admission");
    }
    for(int bad=0;bad<20;++bad){Bridge b;prime(b);world_fixture.saves=1;
        switch(bad){case 0:world_fixture.resolver=false;break;case 1:world_fixture.thread=3;break;case 2:world_fixture.image=101;break;
        case 3:world_fixture.sha="other";break;case 4:world_fixture.session=false;break;case 5:world_fixture.context.id=2;break;
        case 6:world_fixture.world_read=false;break;case 7:world_fixture.world=nullptr;break;case 8:world_fixture.state=nullptr;break;
        case 9:world_fixture.epoch=6;break;case 10:world_fixture.generation=9;break;case 11:world_fixture.status_read=false;break;
        case 12:world_fixture.common=0;break;case 13:world_fixture.common=10;break;case 14:world_fixture.mode=2;break;
        case 15:world_fixture.mode_state=3;break;case 16:world_fixture.present=false;break;case 17:world_fixture.quitting=true;break;
        case 18:world_fixture.title=true;break;case 19:world_fixture.map=true;break;}
        check(!b.tick(0,false,e,now)&&retained(b)&&b.takes==0&&!e.empty(),"busy save cannot hide resolver/session/world/epoch/active-state failure");
        check(b.before_ready(1,result,0,e)==Ready::Rejected&&retained(b)&&b.takes==0,"HostResult rejects same fresh invalid owner or boundary");
        if(bad==11)check(e=="original world status failure","Client readiness preserves upstream read error");
    }
    {Bridge b;prime(b);world_fixture.saves=1;admission_result=Admission::Duplicate;
        check(b.tick(0,false,e,now)&&b.takes==0&&b.port_.application_probes==0&&!b.queued_,"duplicate Frame consumes queue without native work or unnecessary save wait");
    }
    {Bridge b;prime(b);world_fixture.saves=1;admission_result=Admission::Wait;
        check(b.before_ready(1,result,0,e)==Ready::Wait&&b.port_.application_probes==0&&retained(b),"existing authority wait remains before native readiness");
    }
    {Bridge b;prime(b);b.take_ok=false;
        check(!b.tick(0,false,e,now)&&e=="original take failure"&&b.takes==1&&retained(b),"failure after admission remains failure, never converted to save wait");
    }
    native::WorldStatus status;
    check(party_guard(status,false,e),"Party original idle boundary allowed");
    status.saves_in_progress=1;
    check(party_guard(status,true,e),"Party own held SaveScope requires one native save");
    check(!party_guard(status,false,e)&&e.find("saves=1 required_saves=0 held=0")!=std::string::npos,"Party entry error distinguishes native save from held scope");
    status.saves_in_progress=0;
    check(!party_guard(status,true,e)&&e.find("saves=0 required_saves=1 held=1")!=std::string::npos,"Party held error reports missing own SaveScope");
    for(int bad=0;bad<7;++bad){status={};switch(bad){case 0:status.active_mode_index=2;break;case 1:status.in_game_state=3;break;
        case 2:status.world_present=false;break;case 3:status.quitting=true;break;case 4:status.title_transition_pending=true;break;
        case 5:status.map_task_pending=true;break;case 6:status.edit_operations=1;break;}
        check(!party_guard(status,false,e)&&e.find("required_mode=3")!=std::string::npos&&e.find("required_edits=0")!=std::string::npos,"every original Party boundary predicate remains strict and reports measured/required values");
    }
    world_fixture={};admission_result=Admission::New;
}
