template<class Need> void boundary_cases(const Need& need){
    // Use the existing PageReadScope fixture pattern with an owned OS page.
    // The generated production update/original bodies drive normalization.
    void* allocation=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    need(allocation!=nullptr,"Toon boundary owned page allocation");
    try{
        for(int mode=0;mode<4;++mode){
            DWORD prior=0;
            need(VirtualProtect(allocation,4096,PAGE_READWRITE,&prior)!=0,"Toon boundary page reset");
            *static_cast<uint32_t*>(allocation)=0;
            fixture::reset();fixture::seed.bits=0;
            fixture::seed.original_ok=mode!=2;
            fixture::seed.throw_original=mode==3;
            unsigned reads=0,copies=0,effects=0;
            const DWORD protection=mode==0?PAGE_READONLY:PAGE_NOACCESS;
            timing::PageReadScope* before_scope=nullptr;
            fixture::backlog_probe=[&](std::string& error){
                ++reads;MEMORY_BASIC_INFORMATION info{};
                if(!effects)before_scope=timing::page_read_scope;
                if(timing::virtual_query(allocation,&info,sizeof info,timing::Part::QueryPlacement)!=sizeof info){
                    error="Toon boundary metadata query failed";return false;
                }
                // Check freshness before copying so the negative control cannot
                // dereference a page that the injected native call protected.
                if(effects&&info.Protect!=protection){error="Toon boundary stale post-original metadata";return false;}
                if(info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))){
                    error="Toon boundary page unreadable";return false;
                }
                uint32_t actual=1;std::memcpy(&actual,allocation,sizeof actual);++copies;
                if(actual!=0){error="Toon boundary live bytes differ";return false;}return true;
            };
            fixture::original_effect=[&](){
                ++effects;
                need(reads==2&&copies==2,"Toon original entry changed pre-call reads");
                need(timing::totals[size_t(timing::Part::QueryPlacement)].count==1,"pre-call reads lost metadata reuse");
                need(timing::page_read_scope&&timing::page_read_scope!=before_scope,"actual native wrapper lacks separate original boundary");
                DWORD old=0;
                need(VirtualProtect(allocation,4096,protection,&old)!=0,"Toon original callback page mutation");
            };
            {
                timing::Scope take(timing::Part::Take);
                fixture::update(1,0);
                need(effects==1&&fixture::observed.originals==1,"Toon original replayed or skipped");
                need(timing::totals[size_t(timing::Part::OriginalNestedReset)].count==2,"original boundary did not invalidate on entry and exit");
                need(timing::page_read_scope==nullptr,"actual Toon wrapper leaked metadata scope");
                if(mode==0){
                    need(fixture::last.confirmed&&!fixture::last.unknown&&reads==3&&copies==3&&fixture::observed.pins==3,
                        "readable mutation changed exact post-call reads/owner confirmation");
                    need(timing::totals[size_t(timing::Part::QueryPlacement)].count==2,"post-call readable metadata was not queried fresh");
                }else if(mode==1){
                    need(!fixture::last.confirmed&&fixture::last.unknown&&fixture::last.original_returned&&reads==3&&copies==2,
                        "protected post-call page did not stop before data copy");
                    need(fixture::last.cause=="Toon boundary page unreadable"&&fixture::observed.pins==2,
                        "post-original page rejection lost cause or continued owner reads");
                    need(timing::totals[size_t(timing::Part::QueryPlacement)].count==2,"post-call unreadable metadata was not queried fresh");
                }else{
                    need(fixture::last.unknown&&!fixture::last.confirmed&&!fixture::last.original_returned&&reads==2&&copies==2,
                        "failed original changed no-replay/post-read behavior");
                    need(mode==2?fixture::last.cause=="Toon original zero-time entry exception=123":
                        fixture::last.cause.find("original exception")!=std::string::npos,"original failure cause changed");
                }
            }
            need(timing::take_depth==0&&timing::page_read_scope==nullptr,"Toon boundary unwind leaked context");
            fixture::original_effect={};fixture::backlog_probe={};
        }
    }catch(...){
        fixture::original_effect={};fixture::backlog_probe={};
        if(!VirtualFree(allocation,0,MEM_RELEASE))std::cerr<<"Toon boundary allocation release failed\n";
        throw;
    }
    need(VirtualFree(allocation,0,MEM_RELEASE)!=0,"Toon boundary allocation release");
}
