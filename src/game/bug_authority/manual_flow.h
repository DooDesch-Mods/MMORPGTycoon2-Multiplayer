#pragma once
#include "job_calls.h"
namespace mt2mp::game::bug_authority::manual_action {
struct CallReceipt {
    bool admitted=false,attempted=false,confirmed=false;
    JobCall job,manual;
    std::string cause;
};
template<class Calls>
bool execute_host(Calls& calls,CallReceipt& receipt,std::string& e){
    if(receipt.admitted){e="BugButton native receipt already admitted; no replay";return false;}
    if(!calls.pin(e))return false;
    receipt.admitted=true;
    try{
        calls.job(receipt.job);
        receipt.attempted|=receipt.job.attempted;
        if(!receipt.job.attempted||!receipt.job.returned||receipt.job.exception)
            e="BugButton original Job method unconfirmed SEH="+std::to_string(receipt.job.exception);
        else if(calls.after_job(e)){
            calls.manual(receipt.manual);receipt.attempted|=receipt.manual.attempted;
            if(!receipt.manual.attempted||!receipt.manual.returned||!receipt.manual.record_confirmed||!receipt.manual.accepted||
               !receipt.manual.cleanup_confirmed||receipt.manual.exception||receipt.manual.cleanup_exception)
                e="BugButton original manualJob property unconfirmed SEH="+std::to_string(receipt.manual.exception)+" cleanup="+std::to_string(receipt.manual.cleanup_exception);
            else if(calls.after_manual(e)){receipt.confirmed=true;return true;}
        }
    }catch(const std::exception& cause){if(e.empty())e=std::string("BugButton original effect exception: ")+cause.what();}
    catch(...){if(e.empty())e="BugButton original effect nonstandard exception";}
    receipt.attempted|=receipt.job.attempted||receipt.manual.attempted;
    receipt.cause=e;return false;
}
// Each successful stage can already have mutated native state. A later failure
// retains that fact for the Controller's Unknown/no-replay decision.
template<class Calls>
bool apply_client(Calls& calls,bool& attempted,std::string& e){
    attempted=false;if(!calls.preflight(e))return false;
    bool part=false;
    try{
        bool ok=calls.bugs(part,e);attempted|=part;if(!ok)return false;
        part=false;ok=calls.developers(part,e);attempted|=part;if(!ok)return false;
        part=false;ok=calls.finish(part,e);attempted|=part;return ok;
    }catch(...){attempted|=part;throw;}
}
}
