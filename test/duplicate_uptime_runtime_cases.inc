void uptime_runtime_cases(){
    namespace record=mt2mp::subscriber_lifecycle;
    auto graph=[](size_t count){record::Graph result;
        result.records={{SIZE_MAX,0,{1,0,"Building"},{}},{0,0,{1,0,"manual"},{{4,37,{}}}},
            {0,1,{1,0,"uptime"},{}},{2,0,{1,0,"history"},{}}};
        for(size_t i=0;i<count;++i)result.records.push_back({3,i,{},{record::Token{4,1,{}}}});
        result.private_links={{1,1000,false},{2,3000,false}};return result;};
    auto owner=[](uintptr_t,std::string& canonical,std::string&){canonical="region:67";return true;};
    std::string error;duplicate_records::Witness old_record,current_record;
    need(duplicate_records::summarize(graph(9),1000,owner,old_record,error),error);
    auto prepare=[&](){reset();auto& target=fixture_message.request.duplicate.rows[0];
        target.record_sha=old_record.sha;target.captured_uptime=std::vector<int32_t>(9,1);
        duplicate_records::UptimeWitness proof;proof.captured=target.captured_uptime;
        need(duplicate_records::summarize_source(graph(10),1000,owner,current_record,error,nullptr,&proof),error);
        fixture_message.evidence.duplicate_before->sources[0].record_sha=current_record.sha;
        fixture_message.evidence.duplicate_after->sources[0].record_sha=current_record.sha;
        fixture_current.state=*fixture_message.evidence.duplicate_before;
        fixture_current.objects.at(target.key).record_sha=current_record.sha;
        fixture_current.uptime.emplace(target.key,std::move(proof));
    };
    prepare();const auto request=fixture_message.request;const auto actual_before=fixture_current.state;
    need(perform(fixture_binding,request,nullptr,error)==Outcome::Confirmed&&original_calls==1&&
        operations.at(request.action_uuid).evidence.duplicate_before==actual_before&&actual_before.sources[0].record_sha!=request.duplicate.rows[0].record_sha,
        "Host did not retain actual current complete Record before its one original Clone");
    Evidence authority;need(observe(fixture_binding,request,authority,error),error);
    need(perform(fixture_binding,request,nullptr,error)==Outcome::Unknown&&original_calls==1,"uptime admission replayed an original Clone");
    prepare();Config::instance().is_host=false;
    need(perform(fixture_binding,request,&authority,error)==Outcome::Confirmed&&original_calls==1,
        "Client independent historical proof rejected the exact adopted Host before Record");
    prepare();Config::instance().is_host=false;fixture_current.state.sources[0].record_sha=old_record.sha;
    need(perform(fixture_binding,request,&authority,error)==Outcome::RejectedNoEffect&&original_calls==0&&
         error=="Duplicate Client complete source/records/RNG/Release/Ledger differ from Host",
         "Client cloned without adopting actual full Host before");
    auto rejected=[&](auto change){prepare();change();error.clear();
        need(perform(fixture_binding,request,nullptr,error)==Outcome::RejectedNoEffect&&original_calls==0&&operations.empty()&&!error.empty(),
            "invalid uptime admission reached original Clone");};
    rejected([&]{fixture_current.uptime.clear();});
    rejected([&]{fixture_current.uptime.at(request.duplicate.rows[0].key).captured=std::vector<int32_t>(8,1);});
    rejected([&]{auto changed=graph(10);changed.records[1].tokens[0].bits++;
        auto& proof=fixture_current.uptime.at(request.duplicate.rows[0].key);duplicate_records::Witness observed;
        need(duplicate_records::summarize_source(changed,1000,owner,observed,error,nullptr,&proof),error);
        fixture_current.state.sources[0].record_sha=observed.sha;});
    rejected([&]{auto& proof=fixture_current.uptime.at(request.duplicate.rows[0].key);duplicate_records::Witness observed;
        auto foreign=[](uintptr_t,std::string& canonical,std::string&){canonical="region:68";return true;};
        need(duplicate_records::summarize_source(graph(10),1000,foreign,observed,error,nullptr,&proof),error);
        fixture_current.state.sources[0].record_sha=observed.sha;});
    rejected([&]{++fixture_current.state.sources[0].region;});
    rejected([&]{fixture_current.state.sources[0].pose[4]=0x3f800000;});
    rejected([&]{fixture_current.state.sources[0].parent="group:"+key(3);});
    rejected([&]{fixture_current.generation++;});
    prepare();tick_binding_ready=true;captures=1;
    auto captured_before=fixture_current;captured_before.state.sources[0].record_sha=old_record.sha;
    captured=Pending{fixture_binding,fixture_action_uuid,{},request.duplicate,captured_before};
    unsigned submissions=0;submit_hook=[&](const std::string&,const DuplicateIntent& observed,std::string&){++submissions;
        need(observed==request.duplicate,"local Frame adoption rewrote captured intent instead of retaining full historical SHA");return true;};
    tick();tick();need(submitted==1&&submissions==1&&rejections==0,"selected uptime adoption did not submit the original intent exactly once");
    for(unsigned kind=0;kind<4;++kind){prepare();tick_binding_ready=true;captures=1;
        captured_before=fixture_current;captured_before.state.sources[0].record_sha=old_record.sha;
        captured=Pending{fixture_binding,fixture_action_uuid,{},request.duplicate,captured_before};
        submit_hook=[](const std::string&,const DuplicateIntent&,std::string&) -> bool {throw std::runtime_error("unrelated pending state change submitted");};
        if(kind==0)fixture_current.state.untouched_sha=std::string(64,'f');
        if(kind==1)++fixture_current.state.random[0];
        if(kind==2)++fixture_current.money.cash;
        if(kind==3)fixture_current.handles.begin()->second.uid++;
        tick();need(submitted==0&&rejections==1,"pending state guard outside selected uptime was weakened");
    }
}
