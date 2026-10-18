#include "monster_remove_fixture.h"
#include "monster_quest_retirement.h"
#include <cstring>
#include <iostream>
namespace qr=mt2mp::game::monster_quest_retirement;
namespace q=mt2mp::npc_quest_records;
struct Memory:mt2mp::npc_census::Memory {
    std::map<uintptr_t,unsigned char> bytes;std::set<uintptr_t> freed;unsigned freed_reads=0;
    void zero(uintptr_t p,size_t n){for(size_t i=0;i<n;++i)bytes[p+i]=0;}
    template<class T>void set(uintptr_t p,const T& v){const auto* b=reinterpret_cast<const unsigned char*>(&v);for(size_t i=0;i<sizeof v;++i)bytes[p+i]=b[i];}
    void string(uintptr_t p,const std::string& s){const auto data=p+16;set(p,data);set(p+8,s.size());for(size_t i=0;i<s.size();++i)bytes[data+i]=static_cast<unsigned char>(s[i]);}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{
        for(auto f:freed)if(p<f+0x230&&p+n>f){++freed_reads;e="freed Quest dereference";return false;}
        auto* b=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){const auto f=bytes.find(p+i);if(f==bytes.end()){e="unreadable fixture";return false;}b[i]=f->second;}return true;
    }
};
constexpr uintptr_t image=0x100000000,fixture_manager=0x10000,table=0x11000,global=0x12000;
constexpr uintptr_t owner_a=0x20000,owner_b=0x22000,array_a=0x24000,array_b=0x25000;
constexpr uintptr_t a0=0x30000,a1=0x31000,a2=0x32000,b0=0x33000,b1=0x34000,target=0x50000,other_target=0x52000;
const std::string target_key=std::string("action:")+birth_id,other_key=std::string("action:")+snapshot_id;
void members(Memory& m,const std::vector<uintptr_t>& rows){
    m.set(fixture_manager+0x38,int32_t(rows.size()));m.zero(table,48);std::array<uintptr_t,2> head{};
    for(size_t i=0;i<rows.size();++i){const auto ptr=rows[i],node=uintptr_t(0x13000+i*32);m.set(global+i*8,ptr);
        const auto bucket=(uint32_t(ptr)*0x9e3779ffu)>>31;m.set(node,ptr);m.set(node+8,uint32_t(ptr));m.set(node+16,head[bucket]);head[bucket]=node;}
    for(size_t i=0;i<2;++i)m.set(table+i*24+16,head[i]);
}
void setup(Memory& m){
    m.set(image+0x13df410,fixture_manager);m.zero(fixture_manager,0x60);m.set(fixture_manager,image+0x17020b0);
    m.set(fixture_manager+0x18,table);m.set(fixture_manager+0x20,int32_t(2));m.set(fixture_manager+0x24,int32_t(31));m.set(fixture_manager+0x30,global);m.set(fixture_manager+0x3c,int32_t(8));m.set(fixture_manager+0x48,int32_t(2));
    for(auto owner:{owner_a,owner_b}){m.zero(owner,0x880);m.set(owner,image+0x1742980);m.set(owner+0x18,uint64_t(owner)+0x100000000ull);
        m.set(owner+0x28,owner+0x1000);m.set(owner+0x1000,owner+0x20);m.set(owner+0x868,owner==owner_a?array_a:array_b);m.set(owner+0x870,int32_t(owner==owner_a?3:2));m.set(owner+0x874,int32_t(4));}
    for(auto object:{target,other_target}){m.set(object+0x18,uint64_t(object)+0x200000000ull);m.set(object+0x28,object+0x1000);m.set(object+0x1000,object+0x20);}
    const std::array<uintptr_t,5> all{a0,a1,a2,b0,b1};
    for(size_t i=0;i<all.size();++i){const auto quest=all[i],owner=i<3?owner_a:owner_b,object=(quest==a1||quest==b0)?other_target:target;
        m.zero(quest,0x230);m.set(quest,image+0x174ac10);m.set(quest+0x84,int32_t(i<3?i:i-3));m.set(quest+0xb8,owner);m.set(quest+0xc0,owner+0x1000);
        m.set(quest+0xf0,object);m.set(quest+0xf8,object+0x1000);m.string(quest+0x38,"Quest"+std::to_string(i));m.string(quest+0x58,"Saved");
        m.set(quest+0x78,int32_t(2));m.set(quest+0x7c,int32_t(i+1));m.set(quest+0x88,int32_t(9));m.set(quest+0x8c,int32_t(4));
        for(auto offset:{0x100,0x170}){m.set(quest+offset,image+0x16e4710);m.set(quest+offset+0x38,image+0x1749370);}
        if(quest==a0||quest==b1)m.set(quest+0x1c4,int32_t(1));
        m.set(quest+0x1e0,image+0x1787870);
        m.set((i<3?array_a:array_b)+(i<3?i:i-3)*8,quest);
    }
    members(m,{a0,b0,a1,a2,b1});
}
bool read(Memory& m,qr::Witness& out,std::string& e,const qr::Witness* before=nullptr){
    const auto owner=[](uintptr_t ptr,uint64_t& uid,std::string& key,std::string&){uid=uint64_t(ptr)+0x100000000ull;key=std::string("action:")+(ptr==owner_a?action_id:snapshot_id);return true;};
    const auto destination=[](uintptr_t ptr,std::string& key,std::string&){key=other_key;return ptr==other_target;};
    return qr::read(m,image,target,target_key,owner,destination,out,e,before);
}
void retire(Memory& m){
    m.set(owner_a+0x870,int32_t(1));m.set(array_a,a1);m.set(a1+0x84,int32_t(0));
    m.set(owner_b+0x870,int32_t(1));m.set(array_b,b0);m.set(b0+0x84,int32_t(0));
    members(m,{a1,b0});m.set(fixture_manager+0x48,int32_t(0));m.freed={a0,a2,b1};
}
int main(){try{unsigned n=0;std::string e;Memory m;setup(m);qr::Witness before,after;
    need(read(m,before,e),e);need(before.removed==std::vector<uintptr_t>({a0,a2,b1}),"global target removal order");++n;
    need(qr::inverse_owners(before)==std::vector<uintptr_t>({owner_a,owner_b}),"same-owner inverse dedup/order");++n;
    auto changed=before;changed.rows[2].record.gold_reward++;need(!qr::unchanged(before,changed,e),"pre-original record drift admitted");++n;
    retire(m);need(read(m,after,e,&before),e);need(qr::survivors(before,after,e)&&qr::absent(m,image,before,e),e);need(!m.freed_reads,"freed Quest reread");++n;
    need(after.rows[0].record.sequence_id==0&&after.rows[1].record.sequence_id==0,"per-owner renumbering");++n;
    auto invalid=[&](auto change){auto bad=after;change(bad);need(!qr::survivors(before,bad,e),"incorrect Quest survivor accepted");++n;};
    invalid([](auto& x){std::swap(x.quests[0],x.quests[1]);});invalid([](auto& x){x.completed++;});
    invalid([](auto& x){x.owners.at(owner_a).state.records[0].sequence_id=1;});
    invalid([](auto& x){x.owners.at(owner_b).state.records[0].gold_reward++;});
    invalid([](auto& x){x.links[0][1]++;});invalid([](auto& x){x.removed={a0};});
    invalid([](auto& x){x.owners.at(owner_b).state.local[0].task=q::TaskStatus::Pending;});
    members(m,{a1,b0,a0});need(!qr::absent(m,image,before,e),"retired pointer still in fixture_manager accepted");++n;
    for(unsigned mode=0;mode<4;++mode){Memory bad;setup(bad);
        if(mode==0)bad.set(a0+0xf8,uintptr_t(0));if(mode==1)bad.set(owner_a+0x1000,uintptr_t(0));
        if(mode==2)bad.set(a1+0x84,int32_t(0));if(mode==3)bad.bytes.erase(a2+0xf0);
        qr::Witness value;e.clear();need(!read(bad,value,e)&&!e.empty(),"invalid Quest ownership or unreadable field accepted");++n;}
    // Exercise the actual inverse reader with all records retained once per owner.
    constexpr uintptr_t inv=0x70000,data=0x71000;Memory inverse;
    inverse.zero(inv,0xe0);inverse.set(inv,image+0x1713330);inverse.set(inv+0xb8,before.owners.at(owner_a).uid);
    inverse.set(inv+0xd0,data);inverse.set(inv+0xd8,int32_t(3));inverse.set(inv+0xdc,int32_t(3));
    const auto& owner=before.owners.at(owner_a);
    for(size_t i=0;i<3;++i){auto ptr=data+i*0x58;const auto& record=owner.state.records[i];inverse.zero(ptr,0x58);
        inverse.set(ptr,record.type);inverse.set(ptr+8,owner.destination_uids[i]);inverse.set(ptr+0x10,record.action);inverse.set(ptr+0x14,record.monsters_to_kill);
        inverse.string(ptr+0x18,record.name);const auto description=uintptr_t(0x73000+i*64);inverse.set(ptr+0x38,description);
        const std::string text="[Quest description goes here]";inverse.set(ptr+0x40,text.size());for(size_t k=0;k<text.size();++k)inverse.bytes[description+k]=static_cast<unsigned char>(text[k]);}
    need(qr::inverse(inverse,image,inv,owner,e),e);++n;
    inverse.set(inv+0xb8,uint64_t(owner_a));need(!qr::inverse(inverse,image,inv,owner,e),"inverse UID high word ignored");++n;
    inverse.set(inv+0xb8,owner.uid);inverse.set(data+0x58+8,uint64_t(1));need(!qr::inverse(inverse,image,inv,owner,e),"inverse survivor target changed");++n;
    // An owner losing its final Quest is still read through the saved owner set.
    Memory single;setup(single);single.set(owner_a+0x870,int32_t(1));single.set(owner_b+0x870,int32_t(0));members(single,{a0});single.set(fixture_manager+0x48,int32_t(1));
    qr::Witness one,zero;need(read(single,one,e),e);single.set(owner_a+0x870,int32_t(0));members(single,{});single.set(fixture_manager+0x48,int32_t(0));single.freed={a0};
    need(read(single,zero,e,&one)&&qr::survivors(one,zero,e)&&qr::absent(single,image,one,e),e);need(zero.owners.size()==1&&!single.freed_reads,"empty owner or freed read");++n;
    std::cout<<n<<" actual Quest read, stable survivors, swap-last order, absence and inverse cases passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
