#include "selection_move_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <cmath>
namespace mt2mp::move_kiss {
bool fail(std::string& e,const char* cause){e=cause;return false;}
#include "selection_move_live_actual.inc"
#include "selection_move_transform_actual.inc"
}
namespace move=mt2mp::move_kiss;namespace read=mt2mp::selection_move_reader;
constexpr uintptr_t fixture_image=0x140000000,fixture_behaviour=0x1000,fixture_records=0x2000;
struct Memory:move::Memory {std::map<uintptr_t,std::vector<uint8_t>> values;
    template<class T>void put(uintptr_t address,const T& value){auto& bytes=values[address];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);}
    bool read(uintptr_t address,void* out,size_t size,std::string& e)override{const auto found=values.find(address);if(found==values.end()||found->second.size()!=size){e="missing fixture memory";return false;}std::memcpy(out,found->second.data(),size);return true;}};
struct Services:move::Services {bool foreign=false;bool main_thread()const noexcept override{return true;}
    bool active(move::Origin&,std::string&)override{return true;}
    bool bind_target(const move::Origin&,uintptr_t,uintptr_t target,uint64_t,move::State& state,std::string& key,move::Family& family,std::string&)override{
        state.alive=true;state.type="SmallInn";state.variant="inn";state.x=100;state.z=100;
        family=foreign?move::Family::Npc:move::Family::Prop;key="target:"+std::to_string(target);return true;}
    bool submit_move(const move::Intent&,std::string&)override{return true;}void reject(const std::string&)noexcept override{}};
Memory memory(){Memory m;m.put(fixture_behaviour,fixture_image+0x172f1f0);m.put(fixture_behaviour+0x401,uint8_t{1});m.put(fixture_behaviour+0x499,uint8_t{0});
    m.put(fixture_behaviour+0x458,fixture_image+0x1747300);m.put(fixture_behaviour+0x460,fixture_records);m.put(fixture_behaviour+0x468,int32_t{2});m.put(fixture_behaviour+0x46c,int32_t{4});
    for(size_t i=0;i<2;++i){const uintptr_t ghost=0x4000+i*0x1000,target=0x8000+i*0x1000,blueprint=0xb000+i*0x1000;
        m.put(fixture_records+i*24,blueprint);m.put(fixture_records+i*24+8,ghost);m.put(fixture_records+i*24+16,target);m.put(target+0x18,uint64_t{55+i});
        m.put(ghost,fixture_image+0x1705230);m.put(ghost+0x210,blueprint);m.put(ghost+0x100,std::array<float,4>{0,0,0,1});m.put(ghost+0x110,110.f);m.put(ghost+0x114,5.f);m.put(ghost+0x118,100.f);m.put(ghost+0x11c,std::array<float,3>{1,1,1});}
    return m;}
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
int main(){try{unsigned cases=0;std::string e;Services services;read::Selection value;move::Origin origin{"snapshot",2,3,0x10};
    unsigned heights=0;auto height=[&](move::Intent& row,uintptr_t blueprint,std::string&){need(blueprint>=0xb000,"wrong blueprint receiver");row.after.y=7;++heights;return true;};
    auto m=memory();need(read::read(m,services,fixture_image,fixture_behaviour,origin,height,value,e),e);need(value.rows.size()==2&&value.rows[1].record==fixture_records+24&&heights==2&&value.rows[1].after.y==7,"complete ordered native read");++cases;
    auto reject=[&](auto mutation){auto bad=memory();mutation(bad);read::Selection ignored;e.clear();need(!read::read(bad,services,fixture_image,fixture_behaviour,origin,height,ignored,e),"invalid record accepted");++cases;};
    reject([](auto& v){v.put(fixture_behaviour+0x499,uint8_t{1});});reject([](auto& v){v.put(fixture_behaviour+0x401,uint8_t{0});});
    reject([](auto& v){v.put(fixture_behaviour+0x468,int32_t{1});});reject([](auto& v){v.put(fixture_behaviour+0x46c,int32_t{1});});
    reject([](auto& v){v.put(fixture_records+24+16,uintptr_t{0x8000});});reject([](auto& v){v.put(0x9018,uint64_t{55});});
    reject([](auto& v){v.put(fixture_records+24,uintptr_t{0xb000});});reject([](auto& v){v.put(0x5000,fixture_image+0x172f1f0);});
    services.foreign=true;need(!read::read(m,services,fixture_image,fixture_behaviour,origin,height,value,e),"foreign family accepted without full binding");++cases;services.foreign=false;
    need(read::read(m,services,fixture_image,fixture_behaviour,origin,height,value,e),e);
    m.put(fixture_behaviour+0x468,int32_t{0});m.put(fixture_behaviour+0x490,uintptr_t{0});m.put(fixture_image+0x13df9d0,uintptr_t{0xd000});m.put(0xd190,uintptr_t{0xe000});
    for(size_t i=0;i<2;++i){m.put(fixture_records+i*24,uintptr_t{0});m.put(fixture_records+i*24+8,uintptr_t{0});}
    need(read::cleaned(m,fixture_image,value,e),e);++cases;m.put(fixture_records+24+8,uintptr_t{0x5000});
    need(!read::cleaned(m,fixture_image,value,e),"second original owned record was not cleaned");++cases;
    std::cout<<cases<<" actual Selection Move reader/cleanup cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
