#include "adapter.h"
#include <algorithm>
#include <limits>
#include <set>
namespace mt2mp::game::developer_staff::bug_creation {
namespace {
template<class T> bool at(const Read& read,uintptr_t object,size_t offset,T& out,std::string& error){
    if(!object||object>std::numeric_limits<uintptr_t>::max()-offset){error="Bug read owner/address invalid";return false;}
    return read(object+offset,&out,sizeof(out),error);
}
bool array(const Read& read,uintptr_t base,size_t offset,std::vector<uintptr_t>& out,std::string& error){
    uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(read,base,offset,data,error)||!at(read,base,offset+8,count,error)||!at(read,base,offset+12,capacity,error))return false;
    // A corrupt capacity must not cause an unbounded diagnostic allocation.
    if(count<0||capacity<count||capacity>1024*1024||(count&&!data)){
        error="Bug array count/capacity/data invalid";return false;
    }
    out.resize(static_cast<size_t>(count));
    if(count&&!read(data,out.data(),out.size()*sizeof(uintptr_t),error))return false;
    std::set<uintptr_t> seen;
    for(auto item:out)if(!item||!seen.insert(item).second){error="Bug array has a null or duplicate receiver";return false;}
    return true;
}
bool hash_membership(const Read& read,uintptr_t map,const std::vector<uintptr_t>& members,std::string& error){
    if(members.empty())return true;
    uintptr_t buckets=0;int32_t capacity=0,shift=0;
    if(!at(read,map,0,buckets,error)||!at(read,map,8,capacity,error)||!at(read,map,12,shift,error))return false;
    if(!buckets||capacity<=0||capacity>1024*1024||shift<0||shift>31){error="Bug tracker hash owner/capacity/shift invalid";return false;}
    for(auto item:members){
        const uint32_t key=static_cast<uint32_t>(item);
        const uint32_t index=(key*0x9e3779ffu)>>static_cast<unsigned>(shift);
        if(index>=static_cast<uint32_t>(capacity)){error="Bug tracker hash index exceeds original capacity";return false;}
        uintptr_t node=0;
        if(!at(read,buckets,static_cast<size_t>(index)*24+16,node,error))return false;
        bool found=false;std::set<uintptr_t> visited;
        while(node){
            if(visited.size()>=members.size()||!visited.insert(node).second){error="Bug tracker hash chain is cyclic or exceeds membership";return false;}
            uintptr_t pointer=0,next=0;uint32_t stored_key=0;
            if(!at(read,node,0,pointer,error)||!at(read,node,8,stored_key,error)||!at(read,node,16,next,error))return false;
            if(pointer==item&&stored_key==key){found=true;break;}
            node=next;
        }
        if(!found){error="Bug tracker vector receiver lacks original hash membership";return false;}
    }
    return true;
}
}
bool observe(const Read& read,const Lease& lease,Snapshot& out,std::string& error){
    if(lease.kind!=Parent::Building&&lease.kind!=Parent::Npc){error="Bug parent family is invalid";return false;}
    if(!lease.image||!lease.parent||!lease.tracker||!lease.lounge){error="Bug local owners missing";return false;}
    uintptr_t collection=0,anchor=0,ap=0,array_ap=0,tracker=0;
    if(!at(read,lease.parent,lease.kind==Parent::Building?0xbd8:0x848,collection,error)||
       !at(read,lease.image,0x13df2e0,tracker,error))return false;
    if(collection!=lease.collection||tracker!=lease.tracker||lease.tracker!=lease.lounge+0x100){
        error="Bug collection parent/type or original Lounge tracker differs";return false;
    }
    Snapshot next;
    if(collection){
        if(!at(read,collection,0,ap,error)||!at(read,collection,0x18,array_ap,error)||!at(read,collection,0x48,anchor,error))return false;
        if(anchor!=lease.parent||ap!=lease.image+0x1705640||array_ap!=lease.image+0x16ef1c0){
            error="Bug collection parent/type or original Lounge tracker differs";return false;
        }
        if(!array(read,collection,0x20,next.collection,error))return false;
    }
    if(!array(read,lease.tracker,0x18,next.all,error)||
       !array(read,lease.tracker,0x40,next.known,error))return false;
    if(!hash_membership(read,lease.tracker,next.all,error)||!hash_membership(read,lease.tracker+0x28,next.known,error))return false;
    for(auto ptr:next.all){
        Bug bug;bug.pointer=ptr;uint8_t known=0,manual=0,debugged=0;
        if(!at(read,ptr,0,ap,error)||!at(read,ptr,0x20,bug.proxy,error)||!at(read,ptr,0x28,bug.runtime_id,error)||
           !at(read,ptr,0x2c,bug.value.type,error)||!at(read,ptr,0x30,bug.value.created,error)||
           !at(read,ptr,0x34,bug.value.discovered,error)||!at(read,ptr,0x38,bug.value.toons,error)||
           !at(read,ptr,0x3c,bug.value.reproducibility,error)||!at(read,ptr,0x40,bug.value.variant,error)||
           !at(read,ptr,0x44,known,error)||!at(read,ptr,0x45,manual,error)||!at(read,ptr,0x46,debugged,error)||
           !at(read,ptr,0x48,bug.job,error)||!at(read,ptr,0x50,bug.anchor,error))return false;
        if(ap!=lease.image+0x1742200||known>1||manual>1||debugged>1){error="Original Bug type or boolean representation differs";return false;}
        bug.value.known=known!=0;bug.value.manual=manual!=0;bug.value.debugged=debugged!=0;
        next.bugs.push_back(bug);
    }
    for(auto ptr:next.collection){
        const auto it=std::find_if(next.bugs.begin(),next.bugs.end(),[&](const Bug& bug){return bug.pointer==ptr;});
        if(it==next.bugs.end()||it->anchor!=lease.parent){error="Bug collection child lacks exact tracker/anchor membership";return false;}
    }
    for(auto ptr:next.known)if(std::find(next.all.begin(),next.all.end(),ptr)==next.all.end()){
        error="Known Bug tracker receiver is absent from all-Bug membership";return false;
    }
    for(const auto& bug:next.bugs)if(bug.value.known!=(std::find(next.known.begin(),next.known.end(),bug.pointer)!=next.known.end())){
        error="Bug known flag and tracker membership differ";return false;
    }
    out=std::move(next);return true;
}
}
