#include "adapter.h"
#include <algorithm>
#include <cmath>
#include <exception>
namespace mt2mp::game::developer_staff::bug_creation {
namespace {
bool fail(std::string& error,const char* cause){error=cause;return false;}
bool appended(const std::vector<uintptr_t>& before,const std::vector<uintptr_t>& after,uintptr_t item){
    return after.size()==before.size()+1&&after.back()==item&&std::equal(before.begin(),before.end(),after.begin());
}
const Bug* find(const Snapshot& state,uintptr_t bug){
    const auto it=std::find_if(state.bugs.begin(),state.bugs.end(),[&](const Bug& b){return b.pointer==bug;});
    return it==state.bugs.end()?nullptr:&*it;
}
bool old_bugs(const Snapshot& before,const Snapshot& after){
    for(const auto& bug:before.bugs){const auto* now=find(after,bug.pointer);if(!now||*now!=bug)return false;}
    return true;
}
bool valid(const Value& value){
    return value.type>=0&&value.type<3&&value.created>=-1&&value.discovered>=-1&&value.toons>=0&&
        value.variant>=0&&std::isfinite(value.reproducibility)&&
        value.reproducibility>=0&&value.reproducibility<=1&&!value.manual&&!value.debugged;
}
bool effect(const Snapshot& before,const Snapshot& after,uintptr_t bug,const Value& value,
            uintptr_t parent,bool inserted,std::string& error){
    const Bug* now=find(after,bug);
    if(!now||now->value!=value||now->job||now->anchor!=parent)return fail(error,"Bug scalar/anchor/no-Job readback differs");
    if(!appended(before.all,after.all,bug)||!old_bugs(before,after))return fail(error,"Bug tracker all-membership or existing Bug state differs");
    if(value.known?!appended(before.known,after.known,bug):before.known!=after.known)
        return fail(error,"Bug tracker known-membership differs");
    if(inserted?!appended(before.collection,after.collection,bug):before.collection!=after.collection)
        return fail(error,"Original Bug collection insertion differs");
    return true;
}
}
bool create(Services& services,const Lease& lease,const Value& value,Receipt& receipt){
    if(receipt.admitted){
        if(receipt.lease!=lease||receipt.requested!=value){receipt.request_rejection="Retained Bug receipt belongs to a different lease or value";return false;}
        return receipt.confirmed;
    }
    if(!valid(value))return fail(receipt.cause,"Creation requires finite original Bug scalars and no manual/debugged Job state");
    if(!lease.collection)return fail(receipt.cause,"Original empty collection attachment is required before Bug insertion");
    if(lease.snapshot.empty()||lease.parent_key.empty()||lease.birth.empty())
        return fail(receipt.cause,"Original snapshot/action parent and Bug birth witness required");
    Snapshot before;
    if(!services.authorize(lease,receipt.cause)||!services.read(lease,before,receipt.cause))return false;
    // Reserve all receipt state before entering an original allocating function.
    receipt.lease=lease;receipt.requested=value;receipt.admitted=true;
    try {
        if(!services.authorize(lease,receipt.cause))return false;
        Snapshot fresh;
        if(!services.read(lease,fresh,receipt.cause))return false;
        if(fresh!=before)return fail(receipt.cause,"Bug owner changed before original insertion");
        Anchor prepare=[&](uintptr_t bug,uintptr_t parent,uintptr_t caller,std::string& error){
            if(receipt.anchor_seen)return fail(error,"Original AddSpecificBug invoked SetAnchor more than once");
            receipt.anchor_seen=true;receipt.bug=bug;
            if(caller!=lease.image+0x59a7f||parent!=lease.parent||!bug)
                return fail(error,"Original AddSpecificBug SetAnchor caller or parent differs");
            Snapshot constructed;
            if(!services.read(lease,constructed,error))return false;
            const Bug* b=find(constructed,bug);
            if(!b||b->value.type!=value.type||b->value.variant!=-1||b->value.discovered!=-1||
               b->value.toons||b->value.known||b->value.manual||b->value.debugged||b->value.reproducibility!=0.1f||
               b->job||b->anchor||b->proxy)
                return fail(error,"Original fresh Bug constructor state differs");
            if(constructed.collection!=before.collection||constructed.known!=before.known||
               !appended(before.all,constructed.all,bug)||!old_bugs(before,constructed))
                return fail(error,"Original Bug constructor tracker delta differs");
            if(!services.scalars(lease,bug,value,error))return false;
            receipt.properties_returned=true;
            if(!services.anchor(lease,bug,error))return false;
            receipt.anchor_returned=true;
            Snapshot anchored;
            return services.read(lease,anchored,error)&&effect(before,anchored,bug,value,parent,false,error);
        };
        uintptr_t result=0;
        receipt.attempted=true;
        receipt.returned=services.add_specific(lease,value,prepare,result,receipt.cause);
        if(!receipt.returned||!receipt.anchor_returned||!result||result!=receipt.bug){
            receipt.unknown=true;
            if(receipt.cause.empty())receipt.cause="Original AddSpecificBug did not return the witnessed inserted Bug";
            return false;
        }
        Snapshot after;
        if(!services.authorize(lease,receipt.cause)||!services.read(lease,after,receipt.cause)||
           !effect(before,after,result,value,lease.parent,true,receipt.cause)){
            receipt.unknown=true;return false;
        }
        receipt.confirmed=true;return true;
    } catch(const std::exception& error){
        receipt.unknown=receipt.attempted;
        if(receipt.cause.empty())receipt.cause=std::string("Bug insertion exception: ")+error.what();
        return false;
    } catch(...){
        receipt.unknown=receipt.attempted;
        if(receipt.cause.empty())receipt.cause="Bug insertion unknown exception";
        return false;
    }
}
}
