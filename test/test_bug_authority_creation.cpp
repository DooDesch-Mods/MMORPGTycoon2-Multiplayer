#include "bug_authority/creation/adapter.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
using namespace mt2mp::game::developer_staff::bug_creation;
namespace {
int checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
Lease lease(){return {0x10000000,0x20000,0x30000,0x40100,0x40000,Parent::Building,"snapshot","action:key","action:birth",2,3};}
Value target(){return {1,1,-1,0,8,0.097191751f,false,false,false};}
struct Fake:Services {
    Snapshot state;
    int calls=0;std::string fault;
    bool authorize(const Lease&,std::string& error)override{if(fault=="owner"){error="owner changed";return false;}return true;}
    bool read(const Lease&,Snapshot& out,std::string&)override{out=state;return true;}
    bool add_specific(const Lease& l,const Value& value,const Anchor& callback,uintptr_t& out,std::string& error)override{
        ++calls;if(fault=="throw")throw std::runtime_error("allocator failure");
        if(fault=="no_callback")return true;
        Bug fresh;fresh.pointer=0x60000;fresh.runtime_id=127;fresh.value.type=value.type;fresh.value.created=1;fresh.value.reproducibility=0.1f;
        if(fault=="ctor")fresh.job=0x999;
        state.all.push_back(fresh.pointer);state.bugs.push_back(fresh);
        const bool ok=callback(fresh.pointer,l.parent,l.image+(fault=="caller"?0x59a80:0x59a7f),error);
        if(fault=="double_callback")callback(fresh.pointer,l.parent,l.image+0x59a7f,error);
        if(fault!="no_insert")state.collection.push_back(fresh.pointer);
        if(fault=="old_mutation")state.bugs.front().value.toons++;
        out=fault=="wrong_return"?0x60008:fresh.pointer;
        return ok&&fault!="double_callback";
    }
    bool scalars(const Lease&,uintptr_t,const Value& value,std::string& error)override{
        if(fault=="property"){error="original property failure";return false;}
        state.bugs.back().value=value;return true;
    }
    bool anchor(const Lease& l,uintptr_t,std::string&)override{
        if(fault=="anchor_throw")throw std::runtime_error("anchor failure");
        state.bugs.back().anchor=l.parent;
        if(state.bugs.back().value.known&&fault!="known_membership")state.known.push_back(state.bugs.back().pointer);
        return true;
    }
};
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;
    template<class T> void put(uintptr_t at,T value){const auto* raw=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[at+i]=raw[i];}
    bool get(uintptr_t at,void* out,size_t count,std::string& error){
        auto* raw=static_cast<uint8_t*>(out);for(size_t i=0;i<count;++i){const auto it=bytes.find(at+i);if(it==bytes.end()){error="original memory read failure";return false;}raw[i]=it->second;}return true;
    }
};
void memory_tests(){
    const auto l=lease();Memory m;
    m.put(l.parent+0xbd8,l.collection);m.put(l.collection,l.image+0x1705640);m.put(l.collection+0x18,l.image+0x16ef1c0);
    m.put(l.collection+0x48,l.parent);m.put(l.image+0x13df2e0,l.tracker);
    for(auto at:{l.collection+0x20,l.tracker+0x18,l.tracker+0x40}){m.put(at,uintptr_t(0));m.put(at+8,int32_t(0));m.put(at+12,int32_t(4));}
    auto read=[&](uintptr_t at,void* out,size_t count,std::string& error){return m.get(at,out,count,error);};
    Snapshot out;std::string error;
    check(observe(read,l,out,error),"empty original collection/Tracker read");
    auto missing=l;missing.collection=0;m.put(l.parent+0xbd8,uintptr_t(0));
    check(observe(read,missing,out,error)&&out.collection.empty(),"missing collection retains tracker read");
    m.put(l.parent+0xbd8,l.collection);
    m.put(l.collection+0x48,uintptr_t(5));check(!observe(read,l,out,error)&&error.find("parent/type")!=std::string::npos,"wrong parent cause");
    m.put(l.collection+0x48,l.parent);m.put(l.tracker+0x20,int32_t(5));
    check(!observe(read,l,out,error)&&error.find("count/capacity")!=std::string::npos,"capacity guard");
    m.put(l.tracker+0x20,int32_t(0));m.bytes.erase(l.collection+0x48);
    check(!observe(read,l,out,error)&&error=="original memory read failure","read failure cause preserved");
    m.put(l.collection+0x48,l.parent);
    const uintptr_t bug=0x60000,data=0x70000,buckets=0x80000,node=0x81000;
    for(size_t i=0;i<0x58;++i)m.put(bug+i,uint8_t(0));
    m.put(bug,l.image+0x1742200);m.put(bug+0x50,l.parent);m.put(bug+0x40,int32_t(8));
    m.put(data,bug);m.put(l.collection+0x20,data);m.put(l.collection+0x28,int32_t(1));
    m.put(l.tracker+0x18,data);m.put(l.tracker+0x20,int32_t(1));
    m.put(l.tracker,buckets);m.put(l.tracker+8,int32_t(2));m.put(l.tracker+12,int32_t(31));
    const auto bucket=(static_cast<uint32_t>(bug)*0x9e3779ffu)>>31u;
    m.put(buckets+static_cast<uintptr_t>(bucket)*24+16,node);
    m.put(node,bug);m.put(node+8,static_cast<uint32_t>(bug));m.put(node+16,uintptr_t(0));
    check(observe(read,l,out,error)&&out.bugs.size()==1,"original nonempty hash/vector/parent closure");
    m.put(node+8,uint32_t(7));
    check(!observe(read,l,out,error)&&error.find("hash membership")!=std::string::npos,"hash key divergence remains visible");
    m.put(node+8,static_cast<uint32_t>(bug));m.put(bug+0x44,uint8_t(1));
    check(!observe(read,l,out,error)&&error.find("known flag")!=std::string::npos,"known vector parity guard");
    m.put(bug+0x44,uint8_t(2));
    check(!observe(read,l,out,error)&&error.find("boolean representation")!=std::string::npos,"native boolean guard");
}
}
int main(){try{
    for(bool known:{false,true}){
        Fake f;auto v=target();v.known=known;Receipt r;
        check(create(f,lease(),v,r)&&r.confirmed&&!r.unknown&&r.anchor_returned,"original insertion success");
        check(create(f,lease(),v,r)&&f.calls==1,"confirmed receipt is not replayed");
        auto wrong=lease();wrong.birth="another birth";
        check(!create(f,wrong,v,r)&&f.calls==1&&!r.request_rejection.empty(),"receipt cannot admit another birth");
    }
    for(const auto* fault:{"caller","ctor","property","no_callback","no_insert","wrong_return","throw","anchor_throw","double_callback","known_membership"}){
        Fake f;f.fault=fault;Receipt r;auto v=target();v.known=true;
        check(!create(f,lease(),v,r)&&r.unknown&&r.attempted&&!r.confirmed&&!r.cause.empty(),fault);
        const auto cause=r.cause;check(!create(f,lease(),v,r)&&f.calls==1&&r.cause==cause,"Unknown original receipt retained without replay");
    }
    Fake owner;owner.fault="owner";Receipt rejected;
    check(!create(owner,lease(),target(),rejected)&&!rejected.attempted&&rejected.cause=="owner changed","pre-effect owner rejection");
    Fake old;Bug existing;existing.pointer=0x50000;existing.anchor=lease().parent;existing.value=target();
    old.state={ {existing.pointer},{existing.pointer},{},{existing} };old.fault="old_mutation";Receipt changed;
    check(!create(old,lease(),target(),changed)&&changed.unknown&&changed.cause.find("existing Bug")!=std::string::npos,"existing child preservation");
    Fake invalid;Receipt invalid_receipt;auto v=target();v.reproducibility=std::numeric_limits<float>::quiet_NaN();
    check(!create(invalid,lease(),v,invalid_receipt)&&invalid.calls==0,"nonfinite rejected before effect");
    memory_tests();std::cout<<checks<<" Bug creation checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
