// Included inside subscriber_lifecycle_append, sharing its existing lookup owner.
namespace {
struct DeveloperPairWork {
    DeveloperPairRequest request;
    uintptr_t context=0;
    bool armed=false;
    std::unique_ptr<DeveloperPairWork> retained;
};
std::unique_ptr<DeveloperPairWork> retained_developer_pairs;
bool developer_pair_fresh(void* user,const local::ExternalLink& link,std::string& e){
    auto& w=*static_cast<DeveloperPairWork*>(user);const auto& r=w.request;
    const uintptr_t expected=link.saved_link_id==1?r.job:link.saved_link_id==2?r.developer:0;
    const std::string key=r.building_key+(link.saved_link_id==1?"#job":"#developer");
    if(!r.attach||!expected||link.object!=expected||link.portable_key!=key)
        return fail(e,"Developer borrowed lookup ID/object/key differs");
    return r.fresh(r.user,e);
}
}
bool bind_developer_pair(const DeveloperPairRequest& request,DeveloperPairReceipt& result,std::string& e){
    result={};e.clear();
    if(busy.test_and_set())return fail(e,"Developer pair existing borrowed lookup scope busy");
    struct Busy{~Busy(){busy.clear();}} release;
    auto owner=std::make_unique<DeveloperPairWork>();auto& w=*owner;w.request=request;
    bool unknown=false,ok=false;
    try{
        if(!hook_ready||!image_base||request.image!=image_base||request.thread!=GetCurrentThreadId()||
           !request.developer||!request.job||!request.developer_property||!request.job_property||
           request.building_key.empty()||!request.fresh)return fail(e,"Developer pair actual shared lookup/image/thread/request missing");
        if(!request.fresh(request.user,e))return false;
        uintptr_t old_job=0,old_developer=0;
        if(!at(request.developer,0x4c8,old_job,e)||!at(request.job,0xb8,old_developer,e))return false;
        const uintptr_t wanted_job=request.attach?request.job:0,wanted_developer=request.attach?request.developer:0;
        if(old_job==wanted_job&&old_developer==wanted_developer){result.confirmed=true;return true;}
        if(request.attach?(old_job||old_developer):(old_job!=request.job||old_developer!=request.developer))
            return fail(e,"Developer pair current reciprocal links differ before original Resolve");
        local::CallReceipt call;
        if(!stage::context_call(api.context_construct,&w.context,call,e)||!w.context){unknown=true;
            if(e.empty())e="Developer pair original Context constructor returned no private context";}
        else {
            uintptr_t head=0,hashed=0;
            if(!at(w.context,0x10,head,e)||!at(w.context,0x18,hashed,e)||head||hashed){
                if(e.empty())e="Developer pair fresh Context lookup storage is not empty";
                unknown=true;
            }else{
                std::vector<local::ExternalLink> links;
                if(request.attach)links={{1,request.building_key+"#job",request.job},{2,request.building_key+"#developer",request.developer}};
                if(lookup_scope.arm(request.thread,&w.context,{},std::move(links),developer_pair_fresh,&w,e)){
                    w.armed=true;
                    if(request.fresh(request.user,e)){
                        developer_staff::construction::link_property(request.image,request.developer_property,
                            request.developer,true,request.attach,&w.context,&result.developer);
                        result.attempted=result.developer.attempted;
                        const auto good=[](const developer_staff::construction::Call& c){return c.record_confirmed&&c.returned&&c.accepted&&c.cleanup_confirmed&&!c.exception&&!c.cleanup_exception;};
                        if(good(result.developer)){
                            developer_staff::construction::link_property(request.image,request.job_property,
                                request.job,false,request.attach,&w.context,&result.job);
                            result.attempted=result.attempted||result.job.attempted;
                            if(good(result.job)){
                                uintptr_t actual_job=0,actual_dev=0;
                                ok=at(request.developer,0x4c8,actual_job,e)&&at(request.job,0xb8,actual_dev,e)&&
                                    actual_job==wanted_job&&actual_dev==wanted_developer&&request.fresh(request.user,e);
                                if(!ok&&e.empty())e="Developer original pair reciprocal readback differs";
                            }
                        }
                        if(!ok&&e.empty())e="Developer original pair Load/Resolve/record cleanup unconfirmed; developer SEH="+
                            std::to_string(result.developer.exception)+" cleanup="+std::to_string(result.developer.cleanup_exception)+
                            " job SEH="+std::to_string(result.job.exception)+" cleanup="+std::to_string(result.job.cleanup_exception);
                        if(!ok)unknown=true;
                    }
                }
            }
            if(!unknown){
                local::CallReceipt close;std::string cleanup;
                if(!stage::context_call(api.context_destroy,&w.context,close,cleanup)){
                    e+=(e.empty()?"":"; ")+cleanup;unknown=true;ok=false;
                }else{w.context=0;result.context_closed=true;}
            }
        }
        if(w.armed){result.lookup=lookup_scope.receipt();
            if(result.lookup.failed){e+=(e.empty()?"":"; ")+result.lookup.cause;unknown=true;ok=false;}
            else if(ok&&(result.lookup.original_calls!=2||result.lookup.external_hits!=(request.attach?2u:0u))){
                e="Developer pair original/borrowed lookup counts differ";unknown=true;ok=false;}}
    }catch(const std::exception& cause){e+=(e.empty()?"":"; ")+std::string("Developer pair C++: ")+cause.what();unknown=true;ok=false;}
    catch(...){e+=(e.empty()?"":"; ")+std::string("Developer pair unknown C++ exception");unknown=true;ok=false;}
    if(w.armed){lookup_scope.clear();w.armed=false;}
    if(unknown){w.request.fresh=nullptr;w.request.user=nullptr;w.retained=std::move(retained_developer_pairs);
        retained_developer_pairs=std::move(owner);result.unknown=true;return false;}
    result.confirmed=ok;return ok;
}
