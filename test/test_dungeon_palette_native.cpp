#include "dungeon_palette_record.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2mp::game;
namespace a=mt2mp::action_session;
struct Memory {
    std::array<unsigned char,0x380> cursor{};std::array<unsigned char,0x400> checkbox{};std::array<std::array<unsigned char,0x380>,4> swatches{};
    std::array<unsigned char,0x90> theme{};std::array<std::array<float,4>,4> theme_colors{};std::array<unsigned char,0x200> dungeon{};std::array<uint32_t,5> palette{1,2,3,4,0xfeedcafe};
    template<class T>static void put(auto& bytes,size_t offset,const T& value){std::memcpy(bytes.data()+offset,&value,sizeof value);}
    Memory(){put(cursor,0,uintptr_t(0x173ea20));put(cursor,0x18,uintptr_t(0x173eba8));put(cursor,0x308,uintptr_t(checkbox.data()));put(cursor,0x2c8,uintptr_t(theme.data()));
        put(checkbox,0,uintptr_t(0x16d98c0));put(checkbox,0x346,uint8_t(1));put(theme,0,uintptr_t(0x16ff4d0));put(theme,0x80,uintptr_t(theme_colors.data()));put(theme,0x88,int32_t(4));put(theme,0x8c,int32_t(4));
        for(size_t i=0;i<4;++i){put(cursor,0x2e8+i*8,uintptr_t(swatches[i].data()));put(swatches[i],0,uintptr_t(0x17224a0));put(swatches[i],0x2c8,std::array<float,4>{.1f,.2f,.3f,.4f});theme_colors[i]={float(i),.5f,.6f,1.f};}
        put(dungeon,0x1a0,uintptr_t(0x172bf80));put(dungeon,0x1c0,uintptr_t(palette.data()));put(dungeon,0x1c8,int32_t(5));put(dungeon,0x1cc,int32_t(5));put(dungeon,0x1d0,uint8_t(0));
    }
    bool operator()(uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;}
};
int main()try{size_t checks=0;std::string error;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(checks));};
    for(auto edit:{a::DungeonPaletteEdit::Colors,a::DungeonPaletteEdit::Options}){Memory m;dungeon_palette_record::Input input;
        check(dungeon_palette_record::input(m,0,uintptr_t(m.cursor.data()),edit,input,error),error.c_str());
        check(input.defaults==(edit==a::DungeonPaletteEdit::Options)&&input.colors[0][0]==(input.defaults?0.f:.1f),"Normal effective Checkbox/Theme/Swatch source differs");
        check(input.reset_checkbox==(edit==a::DungeonPaletteEdit::Colors)&&input.refresh_swatches==(edit==a::DungeonPaletteEdit::Options),"Normal Checkbox reset/Theme refresh branch differs");
    }
    const auto invalid=[&](auto mutate){Memory m;mutate(m);dungeon_palette_record::Input input;check(!dungeon_palette_record::input(m,0,uintptr_t(m.cursor.data()),a::DungeonPaletteEdit::Options,input,error),"Foreign native palette input accepted");};
    invalid([](auto& m){Memory::put(m.cursor,0,uintptr_t(1));});invalid([](auto& m){Memory::put(m.cursor,0x18,uintptr_t(1));});invalid([](auto& m){Memory::put(m.checkbox,0,uintptr_t(1));});invalid([](auto& m){Memory::put(m.checkbox,0x346,uint8_t(2));});
    invalid([](auto& m){Memory::put(m.cursor,0x348,uintptr_t(1));});
    for(size_t binding:{size_t(0x3c0),size_t(0x3c8)})invalid([&](auto& m){Memory::put(m.checkbox,binding,uintptr_t(1));});
    invalid([](auto& m){Memory::put(m.cursor,0x2e8,uintptr_t(0));});invalid([](auto& m){Memory::put(m.cursor,0x2f0,uintptr_t(m.swatches[0].data()));});invalid([](auto& m){Memory::put(m.swatches[0],0,uintptr_t(1));});invalid([](auto& m){Memory::put(m.swatches[0],0x370,uintptr_t(1));});
    invalid([](auto& m){Memory::put(m.theme,0,uintptr_t(1));});invalid([](auto& m){Memory::put(m.theme,0x88,int32_t(3));});invalid([](auto& m){Memory::put(m.theme,0x8c,int32_t(3));});invalid([](auto& m){m.theme_colors[0][0]=std::numeric_limits<float>::infinity();});
    for(bool absent:{false,true}){Memory m;if(absent)Memory::put(m.cursor,0x2c8,uintptr_t(0));else Memory::put(m.theme,0x88,int32_t(0));dungeon_palette_record::Input input;check(dungeon_palette_record::input(m,0,uintptr_t(m.cursor.data()),a::DungeonPaletteEdit::Options,input,error)&&input.defaults&&input.colors[0][0]==.1f,"Native missing/zero-color Theme fallback changed");}
    for(int mode=0;mode<10;++mode){Memory m;std::vector<int> trace;dungeon_palette_record::Receipt receipt;a::DungeonPaletteIntent intent;intent.defaults=true;intent.packed={10,20,30,40};
        if(mode==1)Memory::put(m.dungeon,0x1a0,uintptr_t(1));if(mode==2)Memory::put(m.dungeon,0x1c8,int32_t(3));if(mode==3)Memory::put(m.dungeon,0x1cc,int32_t(4));
        auto item=[&](uintptr_t array,int32_t index,uintptr_t& slot,std::string& e){check(array==uintptr_t(m.dungeon.data())+0x1b8,"Foreign original vector receiver");trace.push_back(index);slot=uintptr_t(m.palette.data())+size_t(index)*4;if(mode==4&&index==2)slot+=4;if(mode==5&&index==2){e="GetItem original failure";return false;}return true;};
        auto flag=[&](uintptr_t slot,bool desired,std::string& e){trace.push_back(10);check(slot==uintptr_t(m.dungeon.data())+0x1d0&&desired,"Foreign original bool slot/value");if(mode==6){e="bool record cleanup failure";return false;}Memory::put(m.dungeon,0x1d0,uint8_t(desired));return true;};
        auto color=[&](uintptr_t slot,const std::array<float,4>&,std::string& e){const auto i=size_t((slot-uintptr_t(m.palette.data()))/4);trace.push_back(20+int(i));if(mode==7&&i==2){e="Color original failure";return false;}m.palette[i]=intent.packed[i]+uint32_t(mode==8);if(mode==9)Memory::put(m.dungeon,0x1d0,uint8_t(0));return true;};
        const auto original=m.palette;const bool ok=dungeon_palette_record::apply(m,flag,item,color,0,uintptr_t(m.dungeon.data()),uintptr_t(m.palette.data()),5,intent,receipt,error);
        if(mode==0){check(ok&&receipt.attempted&&receipt.returned&&receipt.flag_calls==1&&receipt.color_calls==4,"Normal explicit-owner original leaf count/readback differs");check(trace==std::vector<int>({0,1,2,3,10,20,21,22,23})&&m.palette.back()==original.back(),"Palette original preflight/order or extra row changed");}
        else {check(!ok&&!receipt.returned,"Invalid/partial palette adapter accepted");if(mode<=5)check(!receipt.attempted&&!receipt.flag_calls&&!receipt.color_calls&&m.palette==original,"Native palette mutated before full slot preflight");
            if(mode==5)check(error=="GetItem original failure","GetItem original cause lost");if(mode==6)check(receipt.attempted&&receipt.flag_calls==1&&!receipt.color_calls&&error=="bool record cleanup failure","Native bool partial failure receipt/cause lost");
            if(mode==7)check(receipt.flag_calls==1&&receipt.color_calls==3&&error=="Color original failure","Native color partial failure receipt/cause lost");}
    }
    std::cout<<checks<<" actual Dungeon palette input/explicit-owner leaf/order/failure comparisons passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
