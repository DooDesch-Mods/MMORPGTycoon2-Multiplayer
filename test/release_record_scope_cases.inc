namespace release_record_fixture {
namespace c=subscriber_lifecycle;namespace release_graph=mt2mp::game::release_graph;
using Key=std::function<bool(uintptr_t,std::string&,std::string&)>;
struct Borrow {c::Memory& memory;const Key& key;release_graph::Value& value;release_graph::Kind kind;};
namespace baseline {
#include "release_record_baseline.inc"
}
namespace actual {
#include "release_record_actual.inc"
}
void check_kind(release_graph::Kind kind){
    Fixture f;Memory memory;release_graph::Value got;std::string e;size_t calls=0;bool key_ok=true;
    const std::string root=c::release_record_root(kind);
    std::memcpy(reinterpret_cast<void*>(f.base+0xe000),root.c_str(),root.size()+1);
    f.put(f.base,int32_t(1));f.put(f.base+8,f.base+0xe000);f.put(f.base+0x10,uint64_t(root.size()));
    f.put(f.base+0x9110,f.base+0xa000);f.put(f.base+0x9118,uint64_t(1));
    f.put(f.base+0xa008,f.base+0xf000);f.put(f.base+0xa010,int32_t(7));
    Key key=[&](uintptr_t object,std::string& value,std::string& cause){
        need(!t::PageReadScope::reader_active(),"Release canonical owner callback borrowed record metadata");
        need(object==f.base+0xf000,"Release scope changed external owner");++calls;
        if(!key_ok){cause="fixture external owner rejected";return false;}value="actual-owner";return true;
    };
    Borrow borrow{memory,key,got,kind};t::Scope measured(t::Part::HostCapture);t::PageReadScope native(t::PageBoundary::OriginalCall);auto* parent=t::page_read_scope;
    const auto check=[&](bool success){
        e.clear();memory.reads.clear();calls=0;auto start=queries();
        const bool old=baseline::inspect(&borrow,reinterpret_cast<void*>(f.base),reinterpret_cast<void*>(f.context),e);
        const auto unscoped=queries()-start;const auto reads=memory.reads;const auto expected=release_graph::encode(got);const auto cause=e;const auto expected_calls=calls;
        e.clear();memory.reads.clear();calls=0;start=queries();
        const bool result=actual::inspect(&borrow,reinterpret_cast<void*>(f.base),reinterpret_cast<void*>(f.context),e);
        const auto scoped=queries()-start;
        need(old==success&&result==old&&e==cause&&release_graph::encode(got)==expected&&memory.reads==reads&&calls==expected_calls,"Release scope changed actual graph/read order/error/owner calls");
        need(t::page_read_scope==parent&&!t::PageReadScope::reader_active(),"Release inspection leaked scope");return std::pair{unscoped,scoped};
    };
    const auto counts=check(true);need(counts.first>1000&&counts.second==1,"Release scope did not reduce repeated OS queries");
    f.put(f.tokens+0x28,uint32_t(777));check(true);need(got.records[1].tokens[0].bits==777,"Release inspection reused token value");
    key_ok=false;check(false);key_ok=true;
    f.put(f.tokens,int32_t(5));check(false);f.put(f.tokens,int32_t(4));
    f.put(f.base+0xa010,int32_t(0));check(false);f.put(f.base+0xa010,int32_t(7));
    f.put(f.base+0x10,uint64_t(1));check(false);f.put(f.base+0x10,uint64_t(root.size()));
    DWORD prior=0;need(VirtualProtect(f.allocation,65536,PAGE_NOACCESS,&prior)!=0,"Release fixture protection failed");check(false);
    DWORD ignored=0;need(VirtualProtect(f.allocation,65536,prior,&ignored)!=0,"Release fixture restore failed");check(true);
    std::cout<<"Release record: unchanged actual graph/owner/error paths, OS queries "<<counts.first<<" -> "<<counts.second<<'\n';
}
void run(){check_kind(release_graph::Kind::Manager);check_kind(release_graph::Kind::PreparedDemands);}
}
