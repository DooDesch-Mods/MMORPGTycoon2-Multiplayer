void management_query_cases(){
    t::totals={};Clock::calls=0;Fake::calls=0;Fake::throws=false;
    MEMORY_BASIC_INFORMATION info{};const auto address=reinterpret_cast<LPCVOID>(uintptr_t(0x123456));
    need(size_t(t::Part::WorldPropScopedOs)==size_t(t::Part::ClientManagementProjectRead)+1,"Management query counters append after all old indices");
    const auto query=[&](t::Part category){
        const auto clocks=Clock::calls;Fake::result=17;Fake::result_error=44;SetLastError(33);
        need(t::detail::query<Fake::query,Clock>(address,&info,sizeof info,category)==17,"Management attribution preserves OS result");
        need(Fake::incoming==33&&GetLastError()==44&&Clock::calls==clocks+2,"Management attribution reuses one timer and preserves LastError");
    };
    {
        t::Scope take(t::Part::Take);t::Scope management(t::Part::ClientManagementApply);
        const auto parent=t::management_query_parts;
        query(t::Part::QueryEngine);
        need(count(t::Part::ClientQueryManagementOtherOs)==1,"unclassified Management work belongs to Other");
        constexpr t::Part children[]={t::Part::ClientManagementPrimaryRead,t::Part::ClientManagementCalendarRead,t::Part::ClientManagementThoughtRead,
            t::Part::ClientManagementThoughtIdentityBuildings,t::Part::ClientManagementFinishRead,t::Part::ClientManagementProjectApply,
            t::Part::ClientManagementSubscribersApply,t::Part::ClientManagementGmApply,t::Part::ClientManagementActorsApply,t::Part::ClientManagementControlApply,
            t::Part::ClientManagementCalendarApply,t::Part::ClientManagementDemandsApply,t::Part::ClientManagementIntroApply,t::Part::ClientManagementThoughtApply,
            t::Part::ClientManagementAnalyticsApply,t::Part::ClientManagementGizmosApply,t::Part::ClientManagementServiceRead,t::Part::ClientManagementGizmoRead,
            t::Part::ClientManagementPrimaryRoots,t::Part::ClientManagementAnalyticsRead,t::Part::ClientManagementIntroRead,t::Part::ClientManagementProjectRead};
        for(size_t i=0;i<std::size(children);++i){
            const auto expected=t::Part(size_t(t::Part::ClientQueryManagementPrimaryReadOs)+i*3);
            const auto scoped=t::Part(size_t(expected)+1),unscoped=t::Part(size_t(expected)+2);
            {
                t::Scope child(children[i]);
                need(t::management_query_parts.all==expected,"each actual child has its own stable appended OS bucket");
                query(t::Part::QueryWorldProp);
                {t::Scope unrelated(t::Part::WorldPropCurrent);query(t::Part::QuerySavedFields);}
                {t::PageReadScope pages;query(t::Part::QueryWorldProp);
                    {t::PageReadScope native(t::PageBoundary::OriginalCall);query(t::Part::QueryWorldProp);}
                    query(t::Part::QueryWorldProp);}
                need(count(expected)==5&&t::totals[size_t(expected)].total_ns==500,"child counts all actual OS queries once using existing elapsed time");
                need(count(scoped)==2&&count(unscoped)==2,"WorldProp distinguishes active reader from absent or original-call scope");
                const auto prior=count(expected);Fake::throws=true;bool caught=false;
                try{t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryWorldProp);}
                catch(const std::runtime_error& e){caught=std::string(e.what())=="original query exception";}Fake::throws=false;
                need(caught&&count(expected)==prior,"throwing query remains visible and has no false completed receipt");
                Fake::result=0;Fake::result_error=ERROR_INVALID_PARAMETER;SetLastError(55);
                need(t::detail::query<Fake::query,Clock>(address,&info,3,t::Part::QueryWorldProp)==0&&GetLastError()==ERROR_INVALID_PARAMETER,"failed OS query retains its original result and cause");
                need(count(expected)==prior+1&&count(unscoped)==3,"completed failed OS query is counted without being called successful");
            }
            need(t::management_query_parts.all==parent.all,"child exit restores Management parent attribution");
        }
        {
            t::Scope primary(t::Part::ClientManagementPrimaryRead);const auto before=count(t::Part::ClientQueryManagementPrimaryReadOs);
            {t::Scope thought(t::Part::ClientManagementThoughtRead);const auto thought_before=count(t::Part::ClientQueryManagementThoughtReadOs);
                {t::Scope identity(t::Part::ClientManagementThoughtIdentityBuildings);query(t::Part::QueryWorldProp);}
                need(count(t::Part::ClientQueryManagementThoughtReadOs)==thought_before,"nested identity OS time is not double counted in Thought parent");
                query(t::Part::QueryWorldProp);}
            need(count(t::Part::ClientQueryManagementPrimaryReadOs)==before,"nested Thought OS time is not double counted in Primary parent");
            query(t::Part::QueryWorldProp);
        }
        try{t::Scope actor(t::Part::ClientManagementActorsApply);throw std::runtime_error("child unwind");}
        catch(const std::runtime_error& e){need(std::string(e.what())=="child unwind","attribution does not suppress child exception");}
        need(t::management_query_parts.all==parent.all,"exception restores Management attribution");
        {t::Scope nested_take(t::Part::Take);const auto before=count(t::Part::ClientQueryManagementTotalOs);query(t::Part::QueryEngine);
            need(count(t::Part::ClientQueryManagementTotalOs)==before,"nested Take does not inherit another Take's Management child");}
        need(t::management_query_parts.all==parent.all,"nested Take restores parent attribution");
        void* owned=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);need(owned!=nullptr,"allocate owned Management query test page");
        const auto queried=count(t::Part::ClientQueryManagementTotalOs),scoped=count(t::Part::WorldPropScopedOs);
        {t::PageReadScope pages;SetLastError(321);
            const auto first=t::virtual_query(owned,&info,sizeof info,t::Part::QueryWorldProp);
            const auto second=t::virtual_query(owned,&info,sizeof info,t::Part::QueryWorldProp);
            const auto last_error=GetLastError();
            need(VirtualFree(owned,0,MEM_RELEASE)!=0,"release only owned Management test page");
            need(first==sizeof info&&second==sizeof info&&last_error==321,"actual query and cache hit preserve result and LastError");
            need(count(t::Part::ClientQueryManagementTotalOs)==queried+1&&count(t::Part::WorldPropScopedOs)==scoped+1,"metadata cache hit adds no false Management or WorldProp OS event");}
        uint64_t calls=0,ns=0;for(size_t i=0;i<=std::size(children);++i){const auto& v=t::totals[size_t(t::Part::ClientQueryManagementOtherOs)+i*3];calls+=v.count;ns+=v.total_ns;}
        need(calls==count(t::Part::ClientQueryManagementTotalOs)&&ns==t::totals[size_t(t::Part::ClientQueryManagementTotalOs)].total_ns,"exclusive child queries partition Management exactly");
        need(count(t::Part::WorldPropScopedOs)+count(t::Part::WorldPropUnscopedOs)==count(t::Part::QueryWorldProp),"WorldProp reader and unscoped queries partition total exactly");
        bool isolated=false;std::thread worker([&]{isolated=t::management_query_parts.all==t::Part::Count&&t::take_depth==0;});worker.join();need(isolated,"Management attribution is thread-local");
    }
    need(t::management_query_parts.all==t::Part::Count,"Take and Management exit restore inactive attribution");
    const auto clocks=Clock::calls,events=count(t::Part::ClientQueryManagementTotalOs);
    {t::Scope child(t::Part::ClientManagementActorsApply);Fake::result=17;t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryWorldProp);}
    need(Clock::calls==clocks&&count(t::Part::ClientQueryManagementTotalOs)==events,"outside Take query remains untimed despite child scope");
    {t::Scope host(t::Part::HostCapture);t::Scope child(t::Part::ClientManagementActorsApply);
        t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryWorldProp);}
    need(count(t::Part::ClientQueryManagementTotalOs)==events&&count(t::Part::HostMemoryVirtualQuery)==1,"Host-only query does not contaminate Client Management buckets");
    const auto snapshot=t::snapshot();
    for(const char* key:{"world_prop_scoped_os","world_prop_unscoped_os","client_query_management_total_os","client_query_management_thought_identity_buildings_os","client_query_management_service_read_world_prop_scoped_os","client_query_management_actors_apply_world_prop_unscoped_os"})
        need(snapshot.contains(key)&&snapshot.at(key).contains("count")&&snapshot.at(key).contains("total_ns")&&snapshot.at(key).contains("max_ns"),"existing snapshot exports exact query fields");
    t::totals={};Clock::calls=0;Fake::calls=0;Fake::result=0;Fake::result_error=0;Fake::throws=false;
}
void phase_partition(){
    uint64_t calls=0,ns=0;
    for(auto p:{t::Part::PlacementUnscopedDeveloperOs,t::Part::PlacementUnscopedTutorialOs,t::Part::PlacementUnscopedEconomyOs,t::Part::PlacementUnscopedOtherOs}){
        calls+=count(p);ns+=t::totals[size_t(p)].total_ns;
    }
    need(calls==count(t::Part::PlacementUnscopedOs),"unscoped high-level groups partition existing count exactly");
    need(ns==t::totals[size_t(t::Part::PlacementUnscopedOs)].total_ns,"unscoped groups reuse exact original elapsed samples");
    uint64_t scoped_calls=0,scoped_ns=0;
    for(auto p:{t::Part::PlacementScopedDeveloperOs,t::Part::PlacementScopedTutorialOs,t::Part::PlacementScopedEconomyOs,t::Part::PlacementScopedOtherOs}){
        scoped_calls+=count(p);scoped_ns+=t::totals[size_t(p)].total_ns;
    }
    need(scoped_calls==count(t::Part::PlacementScopedOs),"scoped high-level groups partition existing count exactly");
    need(scoped_ns==t::totals[size_t(t::Part::PlacementScopedOs)].total_ns,"scoped groups reuse exact original elapsed samples");
    partition();
}
void phase_cases(){
    MEMORY_BASIC_INFORMATION info{};const auto address=reinterpret_cast<LPCVOID>(uintptr_t(0x123456));
    const auto query=[&](t::Part expected){
        const auto previous=count(expected),clock=Clock::calls;const auto ns=t::totals[size_t(expected)].total_ns;
        Fake::result=17;Fake::result_error=44;SetLastError(33);
        need(t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryPlacement)==17,"phase attribution retains original return");
        need(Fake::incoming==33&&GetLastError()==44,"phase attribution preserves exact LastError");
        need(count(expected)==previous+1&&t::totals[size_t(expected)].total_ns==ns+100&&Clock::calls==clock+2,"one original timer feeds selected phase");
        phase_partition();
    };
    {
        t::Scope take(t::Part::Take);query(t::Part::PlacementUnscopedOtherOs);
        const t::Part phases[]={t::Part::DeveloperPreflight,t::Part::DeveloperAdopt,t::Part::TutorialPreflight,t::Part::TutorialApply,t::Part::BeforeDay,t::Part::DayTake,t::Part::AfterDay};
        for(auto phase:phases){
            const auto expected=t::unscoped_phase_part(t::phase_for(phase,t::QueryPhase::Other));
            {t::Scope high(phase);query(expected);
                for(auto low:{t::Part::EconomyRead,t::Part::DayRead,t::Part::Scalar,t::Part::Motion,t::Part::QuestStores,t::Part::NpcStores,t::Part::FullFinal}){
                    t::Scope detail(low);query(expected);
                }
                for(auto other:{t::Part::BugPreflight,t::Part::BugAdopt,t::Part::BugFinish,t::Part::NetworkPreflight,t::Part::NetworkApply,t::Part::Take}){
                    {t::Scope explicit_group(other);query(t::Part::PlacementUnscopedOtherOs);}
                    query(expected);
                }
                const auto before=count(expected),all=count(t::Part::PlacementUnscopedOs);
                {t::PageReadScope reader;const auto scoped=count(t::Part::PlacementScopedOs);
                    t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryPlacement);
                    need(count(t::Part::PlacementScopedOs)==scoped+1&&count(expected)==before&&count(t::Part::PlacementUnscopedOs)==all,"scoped OS query excluded from unscoped groups");
                    const auto scoped_expected=t::scoped_phase_part(t::phase_for(phase,t::QueryPhase::Other));
                    query(scoped_expected);
                    for(auto low:{t::Part::EconomyRead,t::Part::DayRead,t::Part::Scalar,t::Part::QuestStores,t::Part::NpcStores,t::Part::FullFinal}){
                        t::Scope detail(low);query(scoped_expected);
                    }
                    {t::Scope bug(t::Part::BugPreflight);query(t::Part::PlacementScopedOtherOs);}
                    query(scoped_expected);
                    const auto prior=count(scoped_expected),prior_all=count(t::Part::PlacementScopedOs);
                    Fake::result=0;Fake::result_error=ERROR_INVALID_PARAMETER;
                    need(t::detail::query<Fake::query,Clock>(address,&info,3,t::Part::QueryPlacement)==0&&GetLastError()==ERROR_INVALID_PARAMETER,"scoped failed query preserves original return and error");
                    need(count(scoped_expected)==prior+1,"scoped failed completed query belongs to current phase");
                    bool caught=false;Fake::throws=true;
                    try{t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryPlacement);}
                    catch(const std::runtime_error& cause){caught=std::string(cause.what())=="original query exception";}
                    Fake::throws=false;
                    need(caught&&count(t::Part::PlacementScopedOs)==prior_all+1,"scoped original exception has no completed query event");
                    phase_partition();
                }
                const auto other=count(t::Part::QueryEngine);
                t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryEngine);
                need(count(t::Part::QueryEngine)==other+1&&count(expected)==before,"other adapters remain outside placement partition");
                Fake::result=0;Fake::result_error=ERROR_INVALID_PARAMETER;
                need(t::detail::query<Fake::query,Clock>(address,&info,3,t::Part::QueryPlacement)==0&&GetLastError()==ERROR_INVALID_PARAMETER,"failed OS query preserves original failure");
                need(count(expected)==before+1,"failed completed query belongs to current phase");
            }
            need(t::query_phase==t::QueryPhase::Other,"high-level exit restores parent group");phase_partition();
        }
        {t::Scope developer(t::Part::DeveloperAdopt);bool caught=false;
            try{t::Scope tutorial(t::Part::TutorialApply);Fake::throws=true;
                t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryPlacement);
            }catch(const std::runtime_error& e){caught=std::string(e.what())=="original query exception";}
            Fake::throws=false;need(caught&&t::query_phase==t::QueryPhase::Developer,"exception restores enclosing group without suppressing original cause");query(t::Part::PlacementUnscopedDeveloperOs);
            bool isolated=false;std::thread worker([&]{isolated=t::query_phase==t::QueryPhase::Other&&t::take_depth==0;});worker.join();
            need(isolated&&t::query_phase==t::QueryPhase::Developer,"phase context remains thread-local");
        }
        const auto data=t::snapshot();for(const char* key:{"placement_unscoped_developer_os","placement_unscoped_tutorial_os","placement_unscoped_economy_os","placement_unscoped_other_os","placement_scoped_developer_os","placement_scoped_tutorial_os","placement_scoped_economy_os","placement_scoped_other_os"})
            need(data.contains(key)&&data.at(key).contains("count")&&data.at(key).contains("total_ns")&&data.at(key).contains("max_ns"),"existing snapshot exports phase counters");
        phase_partition();
    }
    need(t::take_depth==0&&t::query_phase==t::QueryPhase::Other,"take and phase restore outside apply");
    const auto before=count(t::Part::MemoryVirtualQuery),clock=Clock::calls;
    {t::Scope high(t::Part::DeveloperAdopt);t::detail::query<Fake::query,Clock>(address,&info,sizeof info,t::Part::QueryPlacement);}
    need(count(t::Part::MemoryVirtualQuery)==before&&Clock::calls==clock,"outside Take keeps original untimed behavior");
    t::totals={};Clock::calls=0;Fake::calls=0;Fake::throws=false;
}
