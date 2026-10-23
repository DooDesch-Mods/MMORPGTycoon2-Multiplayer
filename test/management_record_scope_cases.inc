#include "general_project_reader.h"
#include "release_graph.h"
#include <functional>
namespace project_progress_fixture {
namespace p=mt2mp::game::general_project;
namespace a {struct Context {};struct Binding {};}
namespace live {namespace apply_timing=::t;struct Pin {};}
struct Runtime {
    uintptr_t image=0;Memory values;bool context_ok=true;size_t context_calls=0;
    struct Adapter {Memory& values;bool copy(uintptr_t address,void* out,size_t bytes,std::string& e){return values.read(address,out,bytes,e);}} memory{values};
};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool context(Runtime& r,const a::Context&,const a::Binding&,live::Pin&,std::string& e){
    need(!t::PageReadScope::reader_active(),"project context entered scoped pure reader");++r.context_calls;
    return r.context_ok||fail(e,"fixture native context rejected");
}
#include "project_progress_readers.inc"
namespace baseline {
#include "project_progress_baseline.inc"
}
namespace actual {
#include "project_progress_actual.inc"
}
void run(){
    Fixture f;std::memset(f.allocation,0,65536);Runtime r;r.image=f.base-0x13df290;const auto lounge=f.base+0x100,clone=f.base+0x300,list=f.base+0xa000;
    f.put(f.base,lounge);f.put(lounge,r.image+0x16de6c0);f.put(lounge+0xa8,clone);
    f.put(lounge+0xd8,list);f.put(lounge+0xe0,int32_t(100));f.put(lounge+0xe4,int32_t(100));
    const auto source=[&](uintptr_t object,size_t index){
        f.put(object,r.image+0x16e48c0);f.put(object+0x18,int32_t(0));
        const auto name="Tech"+std::to_string(index);std::memcpy(reinterpret_cast<void*>(object+0x70),name.c_str(),name.size()+1);
        f.put(object+0x20,object+0x70);f.put(object+0x28,uint64_t(name.size()));
        f.put(object+0x40,f.base+0xe000);f.put(object+0x48,uint64_t(0));f.put(object+0x60,int32_t(0));
    };
    source(clone,0);for(size_t i=0;i<100;++i){const auto object=f.base+0x1000+i*0x80;source(object,i);f.put(list+i*8,object);}
    p::Progress expected,got;uintptr_t expected_lounge=0,expected_current=0,l=0,c=0;std::string e;
    t::Scope measured(t::Part::HostCapture);t::PageReadScope native(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
    const auto check=[&](bool success,bool validate=true){
        e.clear();r.values.reads.clear();const auto before=queries();
        const bool old=baseline::progress_once(r,{},{},expected,expected_lounge,expected_current,e,validate);
        const auto unscoped=queries()-before;const auto reads=r.values.reads;const auto cause=e;
        e.clear();r.values.reads.clear();const auto start=queries();const bool result=actual::progress_once(r,{},{},got,l,c,e,validate);
        const auto scoped=queries()-start;
        need(old==success&&result==old&&e==cause&&expected==got&&l==expected_lounge&&c==expected_current&&r.values.reads==reads,"project scope changed result/error/bytes/read order");
        need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"project reader leaked scope");
        return std::pair{unscoped,scoped};
    };
    const auto counts=check(true);need(counts.first>1000&&counts.second<=3,"project scope did not reduce repeated OS queries");
    f.put(lounge+0xc4,int32_t(7));check(true);need(got.workers==7,"project reader reused previous values");
    f.put(lounge+0xb4,int32_t(-1));check(false);check(true,false);f.put(lounge+0xb4,int32_t(0));
    f.put(list,clone);check(false);f.put(list,f.base+0x1000);
    f.put(list+8,f.base+0x1000);check(false);f.put(list+8,f.base+0x1080);
    source(f.base+0x1000,101);check(false);source(f.base+0x1000,0);
    f.put(clone+0x28,uint64_t(201));check(false);source(clone,0);
    f.put(lounge,uintptr_t(0));check(false);f.put(lounge,r.image+0x16de6c0);
    r.context_ok=false;check(false);r.context_ok=true;
    DWORD prior=0;need(VirtualProtect(f.allocation,65536,PAGE_NOACCESS,&prior)!=0,"project fixture protection failed");
    check(false);DWORD ignored=0;need(VirtualProtect(f.allocation,65536,prior,&ignored)!=0,"project fixture protection restore failed");check(true);
    f.put(lounge+0xa8,uintptr_t(0));check(true);need(!got.current,"project empty current differs");
    std::cout<<"project progress: unchanged actual reads/errors, OS queries "<<counts.first<<" -> "<<counts.second<<'\n';
}
}
namespace gm_record_fixture {
namespace c=subscriber_lifecycle;namespace release_graph=mt2mp::game::release_graph;
using Key=std::function<bool(uintptr_t,std::string&,std::string&)>;
struct Borrow {c::Memory& memory;const Key& key;release_graph::Value& value;};
namespace baseline {
#include "gm_record_baseline.inc"
}
namespace actual {
#include "gm_record_actual.inc"
}
void run(){
    Fixture f;Memory memory;release_graph::Value got;std::string e;size_t calls=0;bool key_ok=true;
    const std::string root=c::release_record_root(release_graph::Kind::GameMasterTickets);
    std::memcpy(reinterpret_cast<void*>(f.base+0xe000),root.c_str(),root.size()+1);
    f.put(f.base,int32_t(1));f.put(f.base+8,f.base+0xe000);f.put(f.base+0x10,uint64_t(root.size()));
    f.put(f.base+0x9110,f.base+0xa000);f.put(f.base+0x9118,uint64_t(1));
    f.put(f.base+0xa008,f.base+0xf000);f.put(f.base+0xa010,int32_t(7));
    Key key=[&](uintptr_t object,std::string& value,std::string& cause){
        need(!t::PageReadScope::reader_active(),"GM canonical owner callback borrowed record metadata");
        need(object==f.base+0xf000,"GM scope changed external owner");++calls;
        if(!key_ok){cause="fixture external owner rejected";return false;}value="actual-owner";return true;
    };
    Borrow borrow{memory,key,got};t::Scope measured(t::Part::HostCapture);t::PageReadScope native(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
    const auto check=[&](bool success){
        e.clear();memory.reads.clear();calls=0;auto start=queries();
        const bool old=baseline::inspect(&borrow,reinterpret_cast<void*>(f.base),reinterpret_cast<void*>(f.context),e);
        const auto unscoped=queries()-start;const auto reads=memory.reads;const auto expected=release_graph::encode(got);const auto cause=e;const auto expected_calls=calls;
        e.clear();memory.reads.clear();calls=0;start=queries();
        const bool result=actual::inspect(&borrow,reinterpret_cast<void*>(f.base),reinterpret_cast<void*>(f.context),e);
        const auto scoped=queries()-start;
        need(old==success&&result==old&&e==cause&&release_graph::encode(got)==expected&&memory.reads==reads&&calls==expected_calls,"GM scope changed actual graph/read order/error/owner calls");
        need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"GM inspection leaked scope");return std::pair{unscoped,scoped};
    };
    const auto counts=check(true);need(counts.first>1000&&counts.second==1,"GM scope did not reduce repeated OS queries");
    f.put(f.tokens+0x28,uint32_t(777));check(true);need(got.records[1].tokens[0].bits==777,"GM inspection reused token value");
    key_ok=false;check(false);key_ok=true;
    f.put(f.tokens,int32_t(5));check(false);f.put(f.tokens,int32_t(4));
    f.put(f.base+0xa010,int32_t(0));check(false);f.put(f.base+0xa010,int32_t(7));
    f.put(f.base+0x10,uint64_t(1));check(false);f.put(f.base+0x10,uint64_t(root.size()));
    DWORD prior=0;need(VirtualProtect(f.allocation,65536,PAGE_NOACCESS,&prior)!=0,"GM fixture protection failed");check(false);
    DWORD ignored=0;need(VirtualProtect(f.allocation,65536,prior,&ignored)!=0,"GM fixture restore failed");check(true);
    std::cout<<"GM record: unchanged actual graph/owner/error paths, OS queries "<<counts.first<<" -> "<<counts.second<<'\n';
}
}
