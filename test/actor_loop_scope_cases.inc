void actor_loop_scope_cases(const staff::Capture& capture){
    using namespace actual_actor;
    std::string e;std::vector<Actor> expected;State original;
    auto q=queries(),reads=copies;
    for(size_t slot=0;slot<50;++slot){Actor a;need(actor(capture,slot,a,e),e);expected.push_back(a);original.actors.push_back(a.pose);}
    const auto independent_queries=queries()-q,independent_reads=copies-reads;
    q=queries();reads=copies;
    for(int pass=0;pass<4;++pass){
        State actual;std::vector<Actor> actors;
        need(graph_actors(capture,actual,actors,e)&&actors==expected&&actual==original,"actual graph50 preserves every Actor and pose");
        need(timing::page_read_scope==nullptr,"graph50 scope ends before target lookup and Bug-work effects");
        need(verify_actors(capture,actors,e),"actual final50 compares every fresh Actor");
        need(timing::page_read_scope==nullptr,"final50 scope ends before return");
    }
    const auto shared_queries=queries()-q,shared_reads=copies-reads;
    need(shared_reads==independent_reads*8,"400 Actor scans retain all live field and Region reads");
    need(shared_queries<independent_queries*8&&shared_queries<40,"only metadata queries are shared inside each pure50 loop");
    std::cout<<"Actor scans 400 live reads "<<shared_reads<<", OS queries "<<independent_queries*8<<" -> "<<shared_queries<<'\n';
    Actor a;
    need(actor(capture,0,a,e,true)&&a==expected[0]&&timing::page_read_scope==nullptr,"opt-in without outer reader creates and closes its own scope");
    {
        timing::PageReadScope outer;
        need(actor(capture,0,a,e,true),e);
        put(dev(1),0x11c,std::array<float,3>{3,1,1});
        need(actor(capture,1,a,e,true)&&a.pose.scale[0]==3,"shared metadata never caches actual fields within a loop");
        put(dev(1),0x11c,std::array<float,3>{1,1,1});
        const auto reset_before=timing::totals[size_t(timing::Part::ReaderNestedReset)].count;
        need(actor(capture,0,a,e),e);
        need(timing::totals[size_t(timing::Part::ReaderNestedReset)].count>=reset_before+4,"default Actor and Region nesting still invalidates parent");
        DWORD old=0;
        {timing::PageReadScope original_call(timing::PageBoundary::OriginalCall);
            need(VirtualProtect(reinterpret_cast<void*>(dev(0)),4096,PAGE_NOACCESS,&old)!=0,"test original boundary changes protection");}
        e.clear();need(!actor(capture,0,a,e,true)&&e=="error: native placement UI page unreadable","original-call boundary invalidates borrowed metadata");
        DWORD ignored=0;
        {timing::PageReadScope original_call(timing::PageBoundary::OriginalCall);
            need(VirtualProtect(reinterpret_cast<void*>(dev(0)),4096,old,&ignored)!=0,"restore protection through original boundary");}
        need(actor(capture,0,a,e,true),"restored protection observed in reused parent");
    }
    put(dev(49),0x11c,std::array<float,3>{2,1,1});
    need(!verify_actors(capture,expected,e)&&e=="Construction frame capture changed actor movement/Region","final loop still rejects last-Actor change");
    need(timing::page_read_scope==nullptr,"failed final loop closes metadata span");
    put(dev(49),0x11c,std::array<float,3>{1,1,1});
    put(regions+69*0x900,0x834,int32_t(67));State actual;std::vector<Actor> actors;
    need(!graph_actors(capture,actual,actors,e)&&e.find("saved region address point")!=std::string::npos,"full Region census still rejects unrelated duplicate ID");
    need(timing::page_read_scope==nullptr,"failed graph loop closes metadata span");
    put(regions+69*0x900,0x834,int32_t(69));
    need(verify_actors(capture,expected,e),"subsequent loop rereads restored Region and all actors");
}
