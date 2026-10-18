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
