void progress_only_cases(){
    action_session::Context c;action_session::Binding b;std::string e;bool selected=false,attempted=false,unknown=false;
    auto project=[](){State s;research::Row row;row.key="bridges";row.value.project_present=true;row.value.purchased=true;row.value.work=10;row.value.desired_workers=1;s.research=row;return s;};
    const auto before=project();auto after=before;after.research->value.work=11;
    need(research_progress_only(before,after,selected,e)&&selected,"existing Research Project did not select bounded route");
    auto with_staff=after;staff_display::State display;display.developers.push_back({0,6,false});display.active_developers=display.active_tech=1;with_staff.staff_display=display;
    need(research_progress_only(before,with_staff,selected,e)&&selected,"Staff-only presentation delta was not admitted with Project");
    for(int field=0;field<11;++field){auto changed=after;
        if(field==0)changed.initial_complete=true;
        if(field==1)changed.leveltwo_complete=true;
        if(field==2)changed.active.push_back(Cursor{});
        if(field==3)changed.startingpoint=startingpoint::Row{};
        if(field==4)changed.terminal.push_back(Cursor{});
        if(field==5)changed.terminal_startingpoint=startingpoint::Row{};
        if(field==6)changed.version=std::array<int32_t,3>{0,0,100};
        if(field==7)changed.terminal_version=std::array<int32_t,3>{0,0,100};
        if(field==8)changed.lifecycle_order[0][0]=1;
        if(field==9)changed.lifecycle_order[1][1]=2;
        if(field==10)changed.leveltwo_initial_next=1;
        need(research_progress_only(before,changed,selected,e)&&!selected,"unrelated lifecycle metadata bypassed ordinary route");
    }
    pass("only Research and Staff fields can differ for the actual whole-State predicate");
    need(research_progress_only(before,before,selected,e)&&!selected,"equal Research selected Progress-only route");
    auto absent=before;absent.research->value.project_present=false;absent.research->value.purchased=false;absent.research->value.work=0;absent.research->value.desired_workers=0;
    need(research_progress_only(absent,before,selected,e)&&!selected,"Research Create bypassed original lifecycle path");
    auto deployed=before;deployed.research->value.project_present=false;deployed.research->value.deployed=true;deployed.research->value.work=0;deployed.research->value.desired_workers=0;
    deployed.research->value.inventory_count=1;deployed.research->value.release_feature_count=1;deployed.research->value.release_change_count=1;deployed.research->value.release_counter=1;deployed.research->value.commit_bugs=startingpoint::CommitBugs{0,0};
    need(research_progress_only(before,deployed,selected,e)&&!selected,"Research Deploy bypassed original lifecycle path");
    auto bad=after;bad.research->value.release_counter=1;
    need(!research_progress_only(before,bad,selected,e)&&!selected&&e=="Research project frame changed native deployment effects","Research original plan cause was suppressed");
    pass("None/Create/Deploy and invalid Research retain the original route or cause");
    {reset();physical_state=project();auto& row=physical_state.research->value;
        row.work=21600.81640625f;row.lines=30692;row.desired_workers=0;row.release_change_count=1;row.release_counter=560;
        desired_state=physical_state;desired_state.research->value.release_counter=440;
        const auto host_copy=desired_state;
        need(preflight(c,b,desired_state,e)&&desired_state==host_copy,"peer stamp preflight modified Host evidence");
        need(apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&research_calls==0&&
             physical_state.research->value.release_counter==560&&desired_state==host_copy,"actual equal Research rows with 440/560 stamps required native stores");
        desired_state.research->value.work+=1;
        need(apply(c,b,desired_state,attempted,unknown,e)&&attempted&&!unknown&&research_calls==1&&
             physical_state.research->value.work==desired_state.research->value.work&&physical_state.research->value.release_counter==560,
             "Research progress did not complete final readback with local stamp");
        for(int field=0;field<4;++field){auto changed=desired_state;
            if(field==0)++changed.research->value.release_change_count;
            if(field==1)++changed.research->value.release_feature_count;
            if(field==2)++changed.research->value.inventory_count;
            if(field==3)changed.research->value.purchased=false;
            const auto calls=research_calls;
            need(!apply(c,b,changed,attempted,unknown,e)&&!attempted&&!unknown&&research_calls==calls,
                 "semantic Research change bypassed frame guards with unequal stamps");
        }
        pass("real 440/560 stamp rows preserve fresh pins, final readback and meaningful guards");}
    {reset();physical_state=project();physical_state.research->value.release_counter=560;
        desired_state=physical_state;desired_state.research->value.release_counter=440;desired_state.research->value.work+=1;fail_read=2;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&attempted&&unknown&&fault.find("original native state failed")!=std::string::npos,
             "stamp comparison bypassed complete final native read");
        pass("unequal stamp progress retains final failure and Unknown");}
    for(bool deployment:{false,true}){reset();physical_state=project();physical_state.research->value.release_change_count=1;
        physical_state.research->value.release_counter=560;desired_state=physical_state;
        if(deployment){auto& row=desired_state.research->value;row.project_present=false;row.deployed=true;
            row.work=0;row.desired_workers=0;row.inventory_count=row.release_feature_count=1;row.release_change_count=2;
            row.commit_bugs=startingpoint::CommitBugs{0,0};row.release_counter=441;
        }else{physical_state.research->value.project_present=false;physical_state.research->value.purchased=false;
            physical_state.research->value.work=0;physical_state.research->value.desired_workers=0;desired_state.research->value.release_counter=440;}
        State target_copy=desired_state,local;std::vector<NativeCursor> cursors;
        need(preflight_local(c,b,target_copy,local,cursors,e)&&target_copy.research->value.release_counter==560+uint32_t(deployment),
             "Create/Deploy comparison did not bind fresh local stamp");
        research::Plan operation;
        need(research::plan(local.research,target_copy.research,operation,e)&&operation==(deployment?research::Plan::Deploy:research::Plan::Create),
             "Create/Deploy lost exact local effect plan");
        if(deployment){physical_state.research->value.release_counter=UINT32_MAX;target_copy=desired_state;
            need(!preflight_local(c,b,target_copy,local,cursors,e)&&e=="Research local Release counter exhausted before deployment",
                 "local deployment stamp overflow accepted");}
    }
    pass("Create and Deploy retain strict local effect plans after comparison binding");
    {reset();physical_state=before;desired_state=after;display_fail_install=true;
        need(!apply(c,b,desired_state,attempted,unknown,e)&&attempted&&unknown&&research_calls==1&&state_reads==1&&e=="tutorial native Apply failed: original display admission failed","Progress Staff failure did not retain Unknown/cause");
        const auto cause=e;need(!apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&research_calls==1&&e==cause,"failed Staff adoption replayed project writes");
        pass("Progress still performs fresh Staff admission and keeps sticky failure");}
    {reset();physical_state=before;desired_state=after;
        need(apply(c,b,desired_state,attempted,unknown,e)&&attempted&&!unknown&&state_reads==2&&research_calls==1&&physical_state==desired_state,"Progress did not retain exactly full before/final captures");
        need(apply(c,b,desired_state,attempted,unknown,e)&&!attempted&&!unknown&&state_reads==3&&research_calls==1,"confirmed Progress replayed native writes");
        pass("Progress keeps two full captures and equal replay performs no effect");}
    {reset("leveltwo_tutorial",16);fixture_present=true;instances={fixture_instance};load.sequences.insert(fixture_sequence);physical_state=target();physical_state.active[0].next=16;desired_state=target("leveltwo_tutorial",true);
        need(apply(c,b,desired_state,attempted,unknown,e)&&state_reads==3&&attempted&&!unknown&&completes==1&&advances==2,"ordinary lifecycle lost post-research capture or original effects");
        pass("non-Research lifecycle retains three captures and original Advance/Complete");}
}
