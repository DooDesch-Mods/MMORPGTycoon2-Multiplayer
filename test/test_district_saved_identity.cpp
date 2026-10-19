#include "district_saved_identity.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace fixture {
namespace saved=mt2mp::game::district_saved_identity;
size_t checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=p[i];}
    bool read(uintptr_t address,void* output,size_t size,std::string& e){auto* p=static_cast<unsigned char*>(output);for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){e="fixture read absent";return false;}p[i]=found->second;}return true;}
};
constexpr uintptr_t image=0x140000000,manager=0x200000,buffer=0x300000,first=0x400000,second=0x500000,context=0x600000,reader=0x700000;
constexpr uint64_t first_uid=0x100000007,second_uid=0x200000007;
const std::string digest(64,'a');
Memory memory(){
    Memory m;const auto prop=image+0x13bef60;
    m.put(prop,image+0x1710190);m.put(prop+0x38,uint64_t(0x18));m.put(manager,image+0x1719080);
    m.put(manager+0x18,image+0x172d8e0);m.put(manager+0x20,buffer);m.put(manager+0x28,int32_t(2));m.put(manager+0x2c,int32_t(2));
    m.put(buffer,first);m.put(buffer+8,second);
    for(const auto object:{first,second}){m.put(object,image+0x16da000);m.put(object+8,context);}
    m.put(first+0x18,first_uid);m.put(second+0x18,second_uid);return m;
}
bool links(uintptr_t p,int32_t& id,std::string&){id=p==first?17:0;return true;}
void observer(){
    auto m=memory();std::string e;const auto prop=image+0x13bef60;saved::SaveFrame frame;saved::Pool out;
    need(saved::begin_save(m,image,prop,manager,reader,context,links,frame,e),e);
    need(saved::finish_save(m,image,frame,prop,manager,reader,context,true,links,out,e),e);
    need(out.records.size()==2&&out.records[1].child==1,"original child lost");
    need(saved::key(digest,67,out.records[0])==digest+":world:district:17","linked record key");
    need(saved::key(digest,67,out.records[1])==digest+":world:district:inline:region{67}/districtManager/district[1]","inline original record key");
    m.put(buffer,second);m.put(buffer+8,first);
    need(!saved::finish_save(m,image,frame,prop,manager,reader,context,true,links,out,e),"save reorder accepted");
    m=memory();m.put(second+0x18,first_uid);need(!saved::observe(m,image,manager,links,out,e),"duplicate full UID accepted");
    m=memory();m.put(buffer+8,first);need(!saved::observe(m,image,manager,links,out,e),"duplicate pointer accepted");
    m=memory();auto same_link=[](uintptr_t,int32_t& id,std::string&){id=17;return true;};
    need(!saved::observe(m,image,manager,same_link,out,e),"duplicate saved link accepted");
    need(!saved::finish_save(m,image,frame,prop,manager,reader+1,context,true,links,out,e),"foreign writer accepted");
    need(!saved::finish_save(m,image,frame,prop,manager,reader,context,false,links,out,e),"failed original accepted");
    m.put(prop+0x38,uint64_t(0x38));need(!saved::begin_save(m,image,prop,manager,reader,context,links,frame,e),"foreign property layout accepted");
    m=memory();m.put(manager+0x28,int32_t(0));saved::LoadFrame load;
    need(saved::begin_load(m,image,prop,manager,reader,context,load,e),e);
    need(!load.children(saved::load_children_return_rva+1,reader,"district",2,2,e),"foreign caller accepted");
    need(!load.children(saved::load_children_return_rva,reader,"monster",2,2,e),"foreign child label accepted");
    need(load.children(saved::load_children_return_rva,reader,"district",2,2,e),e);
    need(!load.children(saved::load_children_return_rva,reader,"district",2,2,e),"duplicate child header accepted");
    need(saved::registered(m,image,load,saved::load_link_return_rva,context,17,first,e),e);
    need(!saved::registered(m,image,load,saved::load_inline_return_rva,context+1,0,second,e),"foreign context accepted");
    need(!load.finish(m,image,out,e),"missing original registration accepted");
    need(saved::registered(m,image,load,saved::load_inline_return_rva,context,0,second,e),e);
    m.put(manager+0x28,int32_t(2));need(load.finish(m,image,out,e),e);
    m.put(buffer,second);m.put(buffer+8,first);need(!load.finish(m,image,out,e),"load reorder accepted");
    m=memory();need(!saved::begin_load(m,image,prop,manager,reader,context,load,e),"nonempty initial load accepted");
    m.put(manager+0x28,int32_t(0));need(saved::begin_load(m,image,prop,manager,reader,context,load,e),e);
    need(load.children(saved::load_children_return_rva,reader,"district",0,0,e)&&load.finish(m,image,out,e)&&out.records.empty(),"empty original District array rejected");
}
void current(){
    auto m=memory();std::string e;saved::Pool census;need(saved::observe(m,image,manager,links,census,e),e);
    const saved::SavedPool pool{67,census};
    auto lookup=[](uint64_t uid,uintptr_t& p,std::string&){p=uid==first_uid?first:uid==second_uid?second:0;return true;};
    std::vector<saved::Current> rows;
    need(saved::current_rows(digest,pool,census,lookup,rows,e)&&rows.size()==2,e);
    const auto first_key=rows[0].key,second_key=rows[1].key;
    std::swap(census.records[0],census.records[1]);census.records[0].child=0;census.records[1].child=1;
    need(saved::current_rows(digest,pool,census,lookup,rows,e)&&rows[0].key==first_key&&rows[1].key==second_key,"array reorder transferred stable keys");
    census.records.erase(census.records.begin()+1);
    need(saved::current_rows(digest,pool,census,lookup,rows,e)&&rows.size()==1&&rows[0].key==second_key,"removed slot transferred key to survivor");
    census.records[0].uid=7;need(!saved::current_rows(digest,pool,census,lookup,rows,e),"truncated/reused UID accepted");
    census=pool.pool;census.records[0].pointer+=0x100;need(!saved::current_rows(digest,pool,census,lookup,rows,e),"same UID different pointer accepted");
    census=pool.pool;auto wrong=[](uint64_t,uintptr_t& p,std::string&){p=0;return true;};
    need(!saved::current_rows(digest,pool,census,wrong,rows,e),"FindUID disagreement accepted");
    ++census.manager;need(!saved::current_rows(digest,pool,census,lookup,rows,e),"foreign manager accepted");
    need(!mt2mp::game::snapshot_district_key::valid(digest+":world:district:inline:region{67}/districtManager/district[-1]"),"negative child key accepted");
}
}
int main()try{fixture::observer();fixture::current();std::cout<<fixture::checks<<" actual District Save/Load/current identity checks passed\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
