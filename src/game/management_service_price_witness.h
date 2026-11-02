#pragma once
#include "management_service_graph.h"
#include "typed_action/management_action.h"

namespace mt2mp::game::management_service_graph {
// Pure receipt only: the original leaf owns all RNG consumption.
template<class Read,class Hash>bool price_witness(Read& at,Hash& hash,uintptr_t image,const Graph& graph,
    action_session::ManagementPriceWitness& out,std::string& error){
    out={};uintptr_t random=0,after_random=0;std::array<uint64_t,4> before{},after{};int32_t extra=0,after_extra=0;
    if(!at(image,0x164b250,random,error))return false;if(!random)return fail(error,"Service price global RNG owner absent");
    if(!at(random,0,before,error)||!at(random,0x20,extra,error))return false;
    std::string canonical,digest;try{if(!projection(graph,canonical,error))return false;digest=hash(canonical);}
    catch(const std::exception& ex){error=std::string("Service price canonical witness failed: ")+ex.what();return false;}
    catch(...){return fail(error,"Service price canonical witness failed with nonstandard exception");}
    if(digest.size()!=64||!std::all_of(digest.begin(),digest.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))return fail(error,"Service price graph SHA-256 invalid");
    if(!at(image,0x164b250,after_random,error))return false;if(after_random!=random)return fail(error,"Service price RNG owner changed during receipt");
    if(!at(random,0,after,error)||!at(random,0x20,after_extra,error))return false;if(after!=before||after_extra!=extra)return fail(error,"Service price RNG values changed during receipt");
    out.graph_sha256=digest;out.random=before;out.random_extra=extra;error.clear();return true;
}
}
