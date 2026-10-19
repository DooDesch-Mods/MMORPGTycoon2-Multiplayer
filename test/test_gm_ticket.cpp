#include "gm_ticket_state.h"
#include "gm_ticket_reference.h"
#include "gm_ticket_storage.h"
#include "gm_ticket_publication.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace g=mt2mp::game::gm_ticket;
namespace {
unsigned comparisons=0;
void need(bool condition,const std::string& cause){++comparisons;if(!condition)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    template<class T>bool operator()(uintptr_t object,size_t offset,T& out,std::string& error){auto* data=reinterpret_cast<uint8_t*>(&out);
        for(size_t i=0;i<sizeof out;++i){const auto found=bytes.find(object+offset+i);if(found==bytes.end()){error="fixture unreadable native field";return false;}data[i]=found->second;}return true;}
};
constexpr uintptr_t fixture_image=0x10000000,fixture_queue=0x20000,fixture_master=0x30000,fixture_ticket=0x40000,fixture_toon=0x50000,fixture_proxy=0x60000,fixture_region=0x70000,fixture_root=0x80000;
Memory memory(){Memory value;value.put(fixture_image+0x13df360,fixture_queue);value.put(fixture_queue,fixture_image+0x16fdc60);value.put(fixture_queue+0x18,fixture_image+0x16f0720);
    value.put(fixture_queue+0x20,uintptr_t(0x21000));value.put(fixture_queue+0x28,int32_t(1));value.put(fixture_queue+0x2c,int32_t(1));value.put(0x21000,fixture_ticket);
    value.put(fixture_master,fixture_image+0x16e5070);value.put(fixture_master+0x18,uint64_t(77));value.put(fixture_master+0x4ac,int32_t(0));value.put(fixture_master+0x4b0,int32_t(1));value.put(fixture_master+0x4b8,fixture_ticket);
    value.put(fixture_master+0x170,fixture_root);value.put(fixture_master+0x178,fixture_region);value.put(fixture_master+0x110,std::array<float,3>{1,2,3});
    value.put(fixture_master+0x100,std::array<float,4>{0,0,0,1});value.put(fixture_master+0x11c,std::array<float,3>{1,1,1});
    value.put(fixture_master+0x4c0,1.0f);value.put(fixture_master+0x4c8,2.0f);value.put(fixture_master+0x4cc,3.0f);value.put(fixture_master+0x4c4,uint8_t(1));value.put(fixture_master+0x4c5,uint8_t(1));
    value.put(fixture_master+0x368,uint8_t(1));value.put(fixture_master+0x4d0,uint8_t(0));
    value.put(fixture_master+0x498,uintptr_t(0x90000));value.put(0x90000,fixture_image+0x16fbbf0);value.put(0x901e8,std::array<float,3>{7,8,9});value.put(0x90250,uint8_t(1));
    value.put(fixture_ticket,fixture_image+0x1787fe0);value.put(fixture_ticket+0x18,int32_t(1));value.put(fixture_ticket+0x1c,std::array<float,3>{4,5,6});
    value.put(fixture_ticket+0x50,fixture_toon);value.put(fixture_ticket+0x58,fixture_proxy);value.put(fixture_proxy,fixture_toon);value.put(fixture_ticket+0x60,fixture_region);value.put(fixture_ticket+0x68,fixture_master);value.put(fixture_ticket+0x70,uint8_t(1));return value;
}
void reader(){auto value=memory();std::string error;g::Census census;
    auto find=[](uint64_t uid,uintptr_t& found,std::string&){found=uid==77?fixture_master:0;return true;};
    need(g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),error);
    need(!g::read(value,find,fixture_image,fixture_queue,fixture_root+8,{fixture_master},census,error),"foreign current native root accepted");
    need(g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),error);
    need(census.values.size()==1&&census.masters.size()==1&&census.values[0].toon_alive,"whole original queue/master witness missing");
    auto toon_key=[](uintptr_t object,std::string& key,std::string& cause){if(object!=fixture_toon){cause="foreign Toon";return false;}key="toon:17";return true;};
    auto region_key=[](uintptr_t object,std::string& key,std::string& cause){if(object!=fixture_region){cause="foreign Region";return false;}key="region:3";return true;};
    g::State portable;need(g::portable(census,toon_key,region_key,portable,error),error);
    need(portable.masters[0].ticket==0&&portable.tickets[0].master==0&&portable.tickets[0].toon=="toon:17","portable same-frame references differ");
    g::State decoded;need(g::decode(g::encode(portable),decoded,error)&&decoded==portable,error);
    auto wire=g::encode(portable);wire["masters"][0]["active"]=1;need(!g::decode(wire,decoded,error),"numeric active flag accepted");
    wire=g::encode(portable);wire["tickets"][0]["master"]=-1;need(!g::decode(wire,decoded,error),"nonreciprocal assignment accepted");
    wire=g::encode(portable);wire["masters"][0]["slot"]=true;need(!g::decode(wire,decoded,error),"Boolean fixed slot accepted");
    wire=g::encode(portable);wire["tickets"][0]["extra"]=0;need(!g::decode(wire,decoded,error),"unknown Ticket field accepted");
    value.put(fixture_proxy,uintptr_t(0));need(g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error)&&!census.values[0].toon_alive,error);
    need(g::portable(census,toon_key,region_key,portable,error)&&portable.tickets[0].toon.empty(),"expired native weak Toon link did not project null");
    value.put(fixture_proxy,uintptr_t(0x123));need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"foreign weak Toon proxy accepted");value=memory();
    value.put(fixture_ticket+0x68,uintptr_t(0));need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"master-to-Ticket ownership without reciprocity accepted");value=memory();
    value.put(fixture_master+0x4b8,uintptr_t(0));need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"Ticket-to-master ownership without reciprocity accepted");value=memory();
    value.put(fixture_master+0x4ac,int32_t(1));need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"wrong original fixed GM slot accepted");value=memory();
    value.put(fixture_master+0x4c8,std::numeric_limits<float>::infinity());need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"nonfinite watch timer accepted");value=memory();
    value.put(fixture_ticket+0x18,int32_t(4));need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"foreign native Ticket enum accepted");value=memory();
    value.put(fixture_image+0x13df360,uintptr_t(0));need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"foreign native Queue singleton accepted");value=memory();
    value.put(fixture_queue+0x28,int32_t(2));value.put(fixture_queue+0x2c,int32_t(2));value.put(0x21008,fixture_ticket);need(!g::read(value,find,fixture_image,fixture_queue,fixture_root,{fixture_master},census,error),"duplicate native Queue Ticket accepted");
    int32_t number=-1;need(g::reference::id("toon:17","toon:",number,error)&&number==17,error);
    for(const char* key:{"toon:-1","toon:01","toon:","toon:2147483648","toon:17x","region:17"})need(!g::reference::id(key,"toon:",number,error),"foreign or noncanonical Toon ID accepted");
}
void storage(){Memory value;constexpr uintptr_t destination=0xa0000,temporary=0xb0000;
    const g::storage::Value old{0xc0000,uint64_t(2)<<32|1},fresh{0xd0000,uint64_t(3)<<32|2};
    const auto populate=[&]{for(auto object:{destination,temporary}){value.put(object,fixture_image+0x16f0720);value.put(object+0x18,fixture_image+0x16f0788);}
        value.put(destination+8,old.data);value.put(destination+0x10,old.extent);value.put(temporary+8,fresh.data);value.put(temporary+0x10,fresh.extent);};
    populate();std::string error;g::storage::Receipt receipt;unsigned calls=0;bool allow=true;
    auto guard=[&](std::string& cause){if(!allow){cause="fixture saved world retired";return false;}return true;};
    auto exchange=[&](uintptr_t address,uint64_t expected,uint64_t desired,bool& stored,std::string& cause){++calls;uint64_t actual=0;if(!value(address,0,actual,cause))return false;stored=actual==expected;if(stored)value.put(address,desired);return true;};
    need(g::storage::transfer(value,exchange,guard,fixture_image,destination,temporary,old,fresh,receipt,error),error);
    need(receipt.confirmed&&receipt.stored==4&&calls==4,"complete original Ticket storage ownership transfer missing");
    g::storage::Value actual;need(g::storage::read(value,fixture_image,destination,actual,error)&&actual==fresh,error);
    need(g::storage::read(value,fixture_image,temporary,actual,error)&&actual==old,error);
    populate();calls=0;allow=false;need(!g::storage::transfer(value,exchange,guard,fixture_image,destination,temporary,old,fresh,receipt,error)&&calls==0,"retired current world allowed Ticket storage mutation");
    allow=true;value.put(destination+0x18,uintptr_t(0));need(!g::storage::transfer(value,exchange,guard,fixture_image,destination,temporary,old,fresh,receipt,error)&&calls==0,"foreign secondary owning Collection accepted");
    populate();calls=0;auto interrupted=[&](uintptr_t address,uint64_t expected,uint64_t desired,bool& stored,std::string& cause){if(calls==2){++calls;cause="fixture third CAS failed";return false;}return exchange(address,expected,desired,stored,cause);};
    need(!g::storage::transfer(value,interrupted,guard,fixture_image,destination,temporary,old,fresh,receipt,error)&&receipt.attempted==3&&receipt.stored==2&&!receipt.confirmed,"partial storage transfer appeared confirmed");
}
}
int main(){try{reader();storage();std::cout<<comparisons<<" native GM/Ticket census, portable state and owning storage assertions passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
