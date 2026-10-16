bool settle_loaded_client(const action_session::Context& context,const action_session::Binding& binding,
                          bool& attempted,std::string& e){
    attempted=false;
    try{
        Capture before;job_census::Census jobs;
        if(!read_process(before,e)||!client(context,binding,before,e)||!job_census::read(copy,before,jobs,e))return false;
        struct Cache {uintptr_t data=0;int32_t capacity=0;uint8_t box=0,stop_flag=0;};
        std::vector<Actor> expected;std::vector<size_t> settle;std::array<Cache,50> caches{};
        for(size_t slot=0;slot<50;++slot){
            Actor a;if(!actor(before,slot,a,e))return false;expected.push_back(a);
            std::string quiet_error;if(quiet(a,quiet_error))continue;
            // Only a loaded direct waypoint cache is admitted. There are no
            // worker results, Building entrances, roads or path nodes to retire.
            uint8_t box=0,stop_flag=0;uintptr_t data=0;int32_t capacity=0;
            if(!at(before.local.developers[slot],0x2a0,box,e)||!at(before.local.developers[slot],0x47f,stop_flag,e)||
               !at(before.local.developers[slot],0x320,data,e)||!at(before.local.developers[slot],0x32c,capacity,e))return false;
            if(a.current||a.destination||a.task||a.road||a.node||a.path||a.pending||a.moving!=1||
               a.steps!=1||a.index!=0||box>1||stop_flag>1||!data||capacity<a.steps)
                return fail(e,quiet_error+"; loaded direct-cache closure unavailable id="+std::to_string(a.pose.id));
            caches[slot]={data,capacity,box,stop_flag};
            settle.push_back(slot);
        }
        const auto unchanged=[&](bool require_quiet){
            Capture current;job_census::Census queue;
            if(!read_process(current,e)||!client(context,binding,current,e)||!owners(before,current,e))return false;
            if(before.state!=current.state||before.local!=current.local)
                return fail(e,"Loaded Client movement changed full50 state or native manager owners");
            if(!job_census::read(copy,current,queue,e))return false;
            if(queue!=jobs)return fail(e,"Loaded Client movement changed JobQueue or reciprocal links");
            for(size_t slot=0;slot<50;++slot){Actor observed;if(!actor(current,slot,observed,e))return false;
                if(observed!=expected[slot])return fail(e,"Loaded Client movement changed unexpected actor field id="+std::to_string(observed.pose.id));
                if(require_quiet&&!quiet(observed,e))return false;
            }
            for(const auto slot:settle){Cache cache;const auto receiver=current.local.developers[slot];
                if(!at(receiver,0x2a0,cache.box,e)||!at(receiver,0x47f,cache.stop_flag,e)||
                   !at(receiver,0x320,cache.data,e)||!at(receiver,0x32c,cache.capacity,e))return false;
                const auto& wanted=caches[slot];
                if(cache.box!=wanted.box||cache.stop_flag!=wanted.stop_flag||cache.data!=wanted.data||cache.capacity!=wanted.capacity)
                    return fail(e,"Loaded Client movement auxiliary flags or travel-array allocation changed");
            }
            return true;
        };
        if(!unchanged(false))return false;
        if(!settle.empty()){
            struct Body{uintptr_t rva;size_t size;const char* sha;};
            constexpr Body bodies[]={
                {0x3b3740,32,"3ef8984fc7bcd2c89ab2f9d39bea0ff2a211505c3220437090b60256d90710db"},
                {0x3b3210,192,"7148fdbcbc456e6c0f4c369e2b9e67245199dc2f5fd13e05d55219b132f29381"},
                {0xc406b0,16,"02c14027a44e6a99f18861a97e86af62149d93ac17fa1dde7a71984e3b394290"}};
            for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
                if(!memory.copy(before.image+body.rva,bytes.data(),bytes.size(),e))return false;
                if(mt2mp::sha256(bytes)!=body.sha)return fail(e,"Loaded Client movement original body differs RVA="+std::to_string(body.rva));}
        }
        for(const auto slot:settle){
            if(!unchanged(false))return false;
            const auto receiver=before.local.developers[slot];
            Call result;stop_loaded_movement(before.image,receiver,&result);attempted=attempted||result.attempted;
            if(result.exception||!result.returned)return fail(e,"Loaded Client original StopMoving/Clear unconfirmed SEH="+std::to_string(result.exception));
            expected[slot].moving=0;expected[slot].steps=0;
            caches[slot].box=0;caches[slot].stop_flag=0;
            if(!unchanged(false))return false;
        }
        return unchanged(true);
    }catch(const std::exception& cause){e+=(e.empty()?"":"; ")+std::string("Loaded Client movement C++: ")+cause.what();}
    catch(...){e+=(e.empty()?"":"; ");e+="Loaded Client movement unknown C++ exception";}
    return false;
}
