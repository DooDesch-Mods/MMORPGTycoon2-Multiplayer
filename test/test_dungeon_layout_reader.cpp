#include "dungeon_layout_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
int main()try{using namespace mt2mp::game::dungeon_layout_reader;unsigned checks=0;std::string error;
    const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    constexpr uintptr_t image=0x140000000;std::array<unsigned char,0x200> dungeon{};std::array<std::array<unsigned char,0x68>,3> objects{};
    std::array<uintptr_t,3> slots{};std::array<uint32_t,5> palette{0x01234567,0x89abcdef,0xffffffff,0,0x11223344};
    const auto put=[](auto& target,size_t offset,auto value){std::memcpy(target.data()+offset,&value,sizeof value);};
    put(dungeon,0x108,image+0x1706690);put(dungeon,0x80,int32_t(3));put(dungeon,0x128,uintptr_t(slots.data()));put(dungeon,0x130,int32_t(3));put(dungeon,0x134,int32_t(3));
    put(dungeon,0x1c0,uintptr_t(palette.data()));put(dungeon,0x1c8,int32_t(5));put(dungeon,0x1cc,int32_t(5));
    std::map<uintptr_t,std::string> names;
    for(size_t i=0;i<3;++i){slots[i]=uintptr_t(objects[i].data());put(objects[i],0,image+0x1723990);put(objects[i],0x38,image+0x17378b0);
        put(objects[i],0x50,std::array<uint32_t,4>{uint32_t(i),uint32_t(i+1),0xffffffff,0x12345678});put(objects[i],0x60,uint8_t(i!=0));names[slots[i]]=i==0?"":"stone";}
    auto at=[](uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;};
    auto text=[&](uintptr_t base,size_t offset,std::string& value,std::string& e){if(offset!=0x18||!names.contains(base)){e="foreign theme owner";return false;}value=names.at(base);return true;};
    Record actual;Local local;const auto sample=[&]{return read(at,text,image,uintptr_t(dungeon.data()),actual,local,error);};
    check(sample(),"Native owned layout record rejected");check(actual.cells.size()==3&&!actual.cells[0].open&&actual.cells[0].theme.empty()&&actual.cells[1].open&&actual.cells[1].theme=="stone","Native open/theme slot layout differs");
    check(actual.cells[2].colors==std::array<uint32_t,4>{2,3,0xffffffff,0x12345678}&&actual.palette==std::vector<uint32_t>(palette.begin(),palette.end()),"Packed RGBA or extra native palette rows lost");
    check(local.cells==std::vector<uintptr_t>(slots.begin(),slots.end())&&local.palette_count==5,"Actual native cell/palette ownership not retained");
    slots[1]=slots[0];check(!sample()&&actual==Record{}&&local==Local{}&&error.find("aliases")!=std::string::npos,"Aliased owned cells accepted or failure exposed partial output");slots[1]=uintptr_t(objects[1].data());
    put(objects[1],0,image+0x17378b0);check(!sample()&&error.find("Cell/Data class")!=std::string::npos,"Foreign cell class accepted");put(objects[1],0,image+0x1723990);
    put(objects[1],0x38,image+0x1723990);check(!sample(),"Foreign embedded data class accepted");put(objects[1],0x38,image+0x17378b0);
    put(objects[1],0x60,uint8_t(2));check(!sample(),"Non-native open bool accepted");put(objects[1],0x60,uint8_t(1));
    put(dungeon,0x108,image+0x1723990);check(!sample()&&error.find("Layout address point")!=std::string::npos,"Foreign layout owner accepted");put(dungeon,0x108,image+0x1706690);
    put(dungeon,0x80,int32_t(2));check(!sample()&&error.find("slot counts")!=std::string::npos,"Grid/layout count mismatch accepted");put(dungeon,0x80,int32_t(3));
    put(dungeon,0x1c8,int32_t(3));check(!sample()&&error.find("palette owner")!=std::string::npos,"Incomplete native palette accepted");put(dungeon,0x1c8,int32_t(5));
    unsigned header_reads=0;auto changed_header=[&](uintptr_t base,size_t offset,auto& value,std::string& e){if(base==uintptr_t(dungeon.data())&&offset==0x128&&++header_reads==2)put(dungeon,0x134,int32_t(4));return at(base,offset,value,e);};
    check(!read(changed_header,text,image,uintptr_t(dungeon.data()),actual,local,error)&&error.find("ownership changed")!=std::string::npos,"Changed native owner header accepted");put(dungeon,0x134,int32_t(3));
    unsigned first_slot_reads=0;auto changed_slot=[&](uintptr_t base,size_t offset,auto& value,std::string& e){if(base==uintptr_t(slots.data())&&offset==0&&++first_slot_reads==2)slots[0]=slots[1];return at(base,offset,value,e);};
    check(!read(changed_slot,text,image,uintptr_t(dungeon.data()),actual,local,error)&&error.find("slot changed")!=std::string::npos,"Changed native member accepted");slots[0]=uintptr_t(objects[0].data());
    auto failed=[](uintptr_t,size_t,auto&,std::string& e){e="original native layout read cause";return false;};
    check(!read(failed,text,image,uintptr_t(dungeon.data()),actual,local,error)&&error=="original native layout read cause","Native layout read error lost");
    std::cout<<checks<<" actual native Dungeon cell/layout/palette/ownership comparisons passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
