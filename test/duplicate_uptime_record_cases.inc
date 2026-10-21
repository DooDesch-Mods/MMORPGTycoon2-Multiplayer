namespace uptime_record_cases {
rec::Graph graph(const std::vector<int32_t>& values){
    rec::Graph result;
    result.records={{SIZE_MAX,0,{1,0,"Building"},{}},{0,0,{1,0,"manual"},{{4,37,{}}}},
        {0,1,{1,0,"uptime"},{}},{0,2,{1,0,"part"},{}},{2,0,{1,0,"history"},{}},{3,0,{1,0,"layout"},{}}};
    for(size_t i=0;i<values.size();++i)result.records.push_back({4,i,{},{rec::Token{4,uint32_t(values[i]),{}}}});
    const size_t nested=result.records.size();result.records.push_back({5,0,{1,0,"nested"},{}});
    result.records.push_back({nested,0,{1,0,"linkId"},{{4,2,{}}}});
    result.records.push_back({nested,1,{1,0,"art"},{{2,0,"complete private payload"}}});
    result.private_links={{1,100,false},{2,200,true},{3,300,false}};return result;
}
void run(){
    std::string error;dr::Witness captured,actual,unchanged;dr::UptimeWitness proof;
    auto owner=[](uintptr_t pointer,std::string& key,std::string& cause){
        if(pointer!=300){cause="unknown private owner";return false;}key="region:67";return true;};
    const std::vector<int32_t> nine(9,1),ten(10,1);
    const auto first=graph(nine),current=graph(ten);
    need(dr::summarize(first,100,owner,captured,error),error);proof.captured=nine;
    need(dr::summarize_source(current,100,owner,actual,error,nullptr,&proof),error);
    need(proof.actual==ten&&proof.historical_sha==captured.sha&&actual.sha!=captured.sha,
        "uptime-only drift did not reproduce the complete historical Record including shifted inline links");
    need(dr::summarize(current,100,owner,unchanged,error)&&unchanged==actual,
        "historical witness changed actual authoritative Record evidence");
    for(const auto& prior:std::vector<std::vector<int32_t>>{{},nine,ten,{0,1,0,1}}){
        for(const auto& now:std::vector<std::vector<int32_t>>{{},nine,ten,{1,0,1}}){
            need(dr::summarize(graph(prior),100,owner,captured,error),error);proof={prior,{}, {}};
            need(dr::summarize_source(graph(now),100,owner,actual,error,nullptr,&proof)&&proof.historical_sha==captured.sha&&proof.actual==now,
                "empty/growing/shrinking/clamped Boolean history lost full graph ordering");
        }
    }
    need(dr::summarize(first,100,owner,captured,error),error);
    auto changed=current;changed.records[1].tokens[0].bits++;proof={nine,{}, {}};
    need(dr::summarize_source(changed,100,owner,actual,error,nullptr,&proof)&&proof.historical_sha!=captured.sha,
        "changed manual property was concealed by uptime admission");
    changed=current;changed.records.back().tokens[0].text+=" changed";proof={nine,{}, {}};
    need(dr::summarize_source(changed,100,owner,actual,error,nullptr,&proof)&&proof.historical_sha!=captured.sha,
        "changed nested private payload was concealed by uptime admission");
    auto foreign_owner=[](uintptr_t,std::string& key,std::string&){key="region:68";return true;};proof={nine,{}, {}};
    need(dr::summarize_source(current,100,foreign_owner,actual,error,nullptr,&proof)&&proof.historical_sha!=captured.sha,
        "changed external canonical link was concealed by uptime admission");
    auto rejected=[&](auto change){auto bad=current;change(bad);proof={nine,{}, {}};error.clear();
        need(!dr::summarize_source(bad,100,owner,actual,error,nullptr,&proof)&&!error.empty(),
            "malformed original uptime graph was accepted");};
    rejected([](auto& bad){bad.records[2].label.text="foreign";});
    rejected([](auto& bad){bad.records[3].label.text="uptime";});
    rejected([](auto& bad){bad.records[4].label.text="foreign";});
    rejected([](auto& bad){bad.records[2].tokens.push_back({4,1,{}});});
    rejected([](auto& bad){bad.records[6].tokens[0].type=3;});
    rejected([](auto& bad){bad.records[6].tokens[0].bits=2;});
    rejected([](auto& bad){bad.records[6].tokens.push_back({4,1,{}});});
    rejected([](auto& bad){bad.records[6].label.type=1;bad.records[6].label.text="named";});
    rejected([](auto& bad){bad.records[7].child_index=0;});
    rejected([](auto& bad){bad.records.push_back({6,0,{},{}});});
    rejected([](auto& bad){bad.private_links[1].has_inline_definition=false;});
    proof={std::vector<int32_t>(11,1),{}, {}};
    need(!dr::summarize_source(current,100,owner,actual,error,nullptr,&proof),"unbounded captured history accepted");
    proof={std::vector<int32_t>{-1},{}, {}};
    need(!dr::summarize_source(current,100,owner,actual,error,nullptr,&proof),"non-Boolean captured history accepted");
    proof={nine,{}, {}};
    need(!dr::summarize_source(graph(std::vector<int32_t>(11,1)),100,owner,actual,error,nullptr,&proof),"unbounded current history accepted");
    proof={};need(dr::summarize_source(current,100,owner,actual,error,nullptr,&proof)&&proof.actual==ten&&proof.historical_sha.empty(),
        "initial original graph did not supply captured history without a historical proof");
    need(dr::summarize_source(fixture(100),100,[](uintptr_t,std::string& key,std::string&){key="canonical";return true;},actual,error),
        "strict non-uptime caller gained a Building-only graph requirement");
}
}
