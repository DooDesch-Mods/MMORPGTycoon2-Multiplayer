#pragma once
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::game_rules::event_observer {
struct Recipient {
    uintptr_t node=0,next=0,listener=0,vtable=0,callback=0;
    bool operator==(const Recipient&)const=default;
};
struct State {
    uint8_t enabled=0;
    uintptr_t head=0;
    uint64_t count=0;
    std::vector<Recipient> recipients;
    bool operator==(const State&)const=default;
};
template<class Read> bool once(Read&& read,uintptr_t image,State& out,std::string& error){
    out={};
    const auto table=image+0x1a2dd00+59*56;
    if(!read(image,0x13b9820,out.enabled,error)||!read(table,0x10,out.head,error)||!read(table,0x18,out.count,error))return false;
    if(out.enabled>1||out.count>256){error="Feature event59 gate/list extent invalid";return false;}
    std::set<uintptr_t> seen;
    auto node=out.head;
    while(node){
        if(!seen.insert(node).second||seen.size()>256){error="Feature event59 recipient cycle/extent invalid";return false;}
        Recipient row;row.node=node;
        if(!read(node,0,row.next,error)||!read(node,8,row.listener,error))return false;
        if(row.listener&&(!read(row.listener,0,row.vtable,error)||!row.vtable||!read(row.vtable,0,row.callback,error)||!row.callback)){
            if(error.empty())error="Feature event59 recipient virtual target absent";return false;
        }
        out.recipients.push_back(row);node=row.next;
    }
    if(out.recipients.size()!=out.count){error="Feature event59 recipient count differs from native list";return false;}
    return true;
}
template<class Read> bool capture(Read&& read,uintptr_t image,State& out,std::string& error){
    State first;
    if(!once(read,image,first,error)||!once(read,image,out,error))return false;
    if(first!=out){error="Feature event59 recipients changed during double read";return false;}
    return true;
}
}
