#include "subscriber_lifecycle/record_graph.h"
#include "duplicate_owned_history.h"
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace native_record=mt2mp::subscriber_lifecycle;
namespace fixture {
size_t checks=0;void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
struct Memory final:native_record::Memory {
    std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t p,const T& v){const auto* data=reinterpret_cast<const uint8_t*>(&v);
        for(size_t i=0;i<sizeof v;++i)bytes[p+i]=data[i];}
    bool read(uintptr_t p,void* out,size_t n,std::string& e)override{auto* data=static_cast<uint8_t*>(out);
        for(size_t i=0;i<n;++i){const auto found=bytes.find(p+i);if(found==bytes.end()){e="unreadable owned record fixture";return false;}data[i]=found->second;}return true;}
};
constexpr uintptr_t root=0x10000,child=0x11000,tokens=0x12000,children=0x13000,context=0x14000,table=0x15000,link=0x16000;
void label(Memory& m,uintptr_t p,const std::string& s){m.put(p,int32_t(1));m.put(p+8,p+0x100);m.put(p+0x10,uint64_t(s.size()));
    for(size_t i=0;i<=s.size();++i)m.put(p+0x100+i,i<s.size()?s[i]:char(0));}
Memory setup(){Memory m;label(m,root,"BugCollection");label(m,child,"linkId");
    for(auto p:{root,child}){m.put(p+0x70,uint8_t(0));m.put(p+0x38,uintptr_t(0));m.put(p+0x40,int32_t(0));m.put(p+0x44,int32_t(0));
        m.put(p+0x50,uintptr_t(0));m.put(p+0x58,int32_t(0));m.put(p+0x5c,int32_t(0));}
    m.put(root+0x50,children);m.put(root+0x58,int32_t(1));m.put(root+0x5c,int32_t(1));m.put(children,child);
    m.put(child+0x38,tokens);m.put(child+0x40,int32_t(1));m.put(child+0x44,int32_t(1));m.put(tokens,int32_t(4));m.put(tokens+0x28,uint32_t(1));
    m.put(context,table);m.put(table+0x10,link);m.put(table+0x18,uint64_t(1));m.put(link,uintptr_t(0));
    m.put(link+8,uintptr_t(0x17000));m.put(link+0x10,int32_t(1));return m;}
}
int main()try{
    using namespace fixture;auto memory=setup();native_record::Budget budget{10,10,1024,10};std::string error;
    std::vector<native_record::Record> owned;native_record::Graph full;
    auto read=[&](){return native_record::read_owned_record(memory,root,budget,owned,error);};
    need(read(),error);need(owned.size()==2&&owned[0].parent==SIZE_MAX&&owned[1].parent==0&&owned[1].child_index==0&&
        owned[1].label.text=="linkId"&&owned[1].tokens[0].bits==1,"complete owned record/order/token lost");
    need(native_record::read_graph(memory,root,context,budget,full,error)&&full.records.size()==owned.size()&&
        full.private_links.size()==1&&full.private_links[0].has_inline_definition,"existing context manifest path changed");
    std::string original_sha,owned_sha;namespace history=mt2mp::game::duplicate_owned_history;
    need(history::captured(full,original_sha,error)&&history::summarize(owned,owned_sha,error)&&original_sha==owned_sha,
        "original owned payload differs after its private SaveContext is destroyed");
    auto changed=owned;changed[0].label.text="DifferentCollection";
    need(history::summarize(changed,owned_sha,error)&&owned_sha!=original_sha,"owned root label omitted from witness");
    changed=owned;changed[1].tokens[0].bits=2;
    need(history::summarize(changed,owned_sha,error)&&owned_sha!=original_sha,"owned token bits omitted from witness");
    auto external=full;external.private_links[0].has_inline_definition=false;
    need(!history::captured(external,owned_sha,error),"detached original record accepted external pointer reference");
    memory.put(link+0x10,int32_t(2));need(!native_record::read_graph(memory,root,context,budget,full,error),"unbound inline definition admitted by full graph path");
    need(read(),"owned record incorrectly requires an expired SaveContext");
    auto bad=[&](auto change){memory=setup();change();need(!read(),"invalid complete owned record admitted");};
    bad([&](){memory.put(root+0x70,uint8_t(1));});
    bad([&](){memory.put(children,root);});
    bad([&](){memory.put(child,int32_t(3));});
    bad([&](){memory.put(child+0x44,int32_t(0));});
    bad([&](){memory.put(tokens+0x28,uint32_t(0));});
    bad([&](){memory.put(tokens,int32_t(5));});
    memory=setup();budget.records=1;need(!read(),"owned record budget ignored");
    budget={10,0,1024,10};need(!read(),"owned token budget ignored");
    budget={10,10,3,10};need(!read(),"owned string budget ignored");
    std::cout<<checks<<" actual owned-record and unchanged context-manifest cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
