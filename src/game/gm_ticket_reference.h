#pragma once
#include "subscriber_lifecycle/reference_map.h"
#include <charconv>
#include <string_view>

namespace mt2mp::game::gm_ticket::reference {
namespace native=subscriber_lifecycle::first_append;
inline bool fail(std::string& error,const char* cause){error=cause;return false;}
inline bool id(std::string_view key,std::string_view prefix,int32_t& out,std::string& error){
    out=-1;if(!key.starts_with(prefix))return fail(error,"GM canonical reference family differs");key.remove_prefix(prefix.size());
    if(key.empty())return fail(error,"GM canonical reference ID is empty");
    const auto result=std::from_chars(key.data(),key.data()+key.size(),out);
    if(result.ec!=std::errc{}||result.ptr!=key.data()+key.size()||out<0||std::to_string(out)!=key)return fail(error,"GM canonical reference ID is not a canonical nonnegative int32");
    return true;
}
inline bool identify(subscriber_lifecycle::Memory& memory,const native::ReferenceWorld& world,uintptr_t object,
    native::ReferenceKind kind,std::string& out,std::string& error){
    out.clear();native::ReferenceKey key;if(!native::identify_reference(memory,world,object,-1,-1,key,error))return false;
    if(key.kind!=kind)return fail(error,"GM actual external reference has a different native family");
    if(kind==native::ReferenceKind::SubscriberToon)out="toon:"+std::to_string(key.subscriber_id);
    else if(kind==native::ReferenceKind::RootRegion)out="region:"+std::to_string(key.region_id);
    else return fail(error,"GM external reference family is not Toon or Region");
    error.clear();return true;
}
inline bool resolve(subscriber_lifecycle::Memory& memory,const native::ReferenceWorld& world,const std::string& key,
    native::ReferenceKind kind,uintptr_t& out,std::string& error){
    out=0;native::ReferenceKey value;value.kind=kind;int32_t number=-1;
    if(kind==native::ReferenceKind::SubscriberToon){if(!id(key,"toon:",number,error))return false;value.subscriber_id=number;}
    else if(kind==native::ReferenceKind::RootRegion){if(!id(key,"region:",number,error))return false;value.region_id=number;}
    else return fail(error,"GM external reference family is not Toon or Region");
    return native::resolve_reference(memory,world,value,out,error);
}
}
