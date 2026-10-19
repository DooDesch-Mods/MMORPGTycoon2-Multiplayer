#include "test_dungeon_light_fixture.h"
#include "dungeon_light_input.h"
#include <cstring>
#include <iostream>
#include <limits>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(checks));};
    for(bool equal:{false,true})for(bool mode:{false,true})for(int32_t count:{1,3}){equal_apply=equal;sandbox=mode;previous_count=count;
        for(const auto& message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Dungeon light complete schema35 roundtrip differs");}
        const auto invalid=[&](auto mutate){auto m=result();mutate(m);check(!encode(m,bytes,error),"Dungeon light foreign effect/domain accepted");};
        invalid([](auto& m){m.request.dungeon_light.key="foreign";});invalid([](auto& m){m.request.dungeon_light.before_sha256="invalid";});
        invalid([](auto& m){m.request.dungeon_light.light[3]=std::numeric_limits<float>::infinity();});invalid([](auto& m){m.request.dungeon_light.input.colors[0][0]=std::numeric_limits<float>::quiet_NaN();});
        invalid([](auto& m){m.evidence.dungeon_light_after.light[0]+=1;});invalid([](auto& m){m.evidence.dungeon_light_after.dungeon.desired+=1;});invalid([](auto& m){m.evidence.dungeon_light_after.dungeon.price+=1;});
        invalid([](auto& m){m.evidence.dungeon_light_after.dungeon.instances[0].started=false;});invalid([](auto& m){m.evidence.dungeon_light_after.dungeon.instances[1].index=5;});invalid([](auto& m){m.evidence.dungeon_light_after.dungeon.undo_count+=1;});
        invalid([](auto& m){m.evidence.after.cash+=1;});invalid([](auto& m){m.evidence.after.line_counts[0]+=1;});invalid([](auto& m){m.request.dungeon_config.desired=2;});invalid([](auto& m){m.evidence.dungeon_config_after.desired=2;});
        check(dungeon_light_wire_fits(request().request,before(),money(),error),"Normal light packet preflight rejected");auto oversized=before();for(int32_t i=3;i<10000;++i)oversized.dungeon.instances.push_back({i,false});check(!dungeon_light_wire_fits(request().request,oversized,money(),error),"Oversized complete light receipt admitted before effect");
        auto message=request();check(encode(message,bytes,error),error.c_str());auto j=nlohmann::json::parse(bytes);j["request"]["dungeon_light"]["input"]["spinners"].push_back(1);auto malformed=j.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Foreign raw Spinner dimension accepted");
        j=nlohmann::json::parse(bytes);j["request"]["dungeon_light"]["light"].push_back(1);malformed=j.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Foreign authored light dimension accepted");
    }
    using namespace mt2mp::game;dungeon_light_input::Widgets widgets;widgets.spinners={1,2,3,4};widgets.sliders={5,6,7};const auto input=intent().input;std::array<float,23> expected;
    auto convert=[](bool slider,uintptr_t object,int32_t raw,float& out,std::string&){out=float(raw)/(slider?1.0f:object==2?1.0f:100.0f);return true;};
    auto direction=[](float angle,std::array<float,3>& out,std::string&){out={angle,1,-angle};return true;};
    check(dungeon_light_input::calculate(input,widgets,convert,direction,expected,error),"Complete original light mapping failed");
    for(size_t i=0;i<4;++i)check(expected[i]==input.colors[0][i]*2&&expected[7+i]==input.colors[1][i]&&expected[11+i]==input.colors[2][i]&&expected[16+i]==input.colors[3][i],"Original full RGBA intensity/ambient/fog/sky mapping differs");
    check(expected[4]==90&&expected[5]==1&&expected[6]==-90&&expected[15]==5*.0010000000474974513f&&expected[20]==2*.009999999776482582f&&expected[21]==100&&expected[22]==2,"Original direction/fog-distance/gain mapping differs");
    auto fail_convert=[](bool,uintptr_t,int32_t,float&,std::string& e){e="conversion failed";return false;};check(!dungeon_light_input::calculate(input,widgets,fail_convert,direction,expected,error)&&error=="conversion failed","Numeric conversion failure suppressed");
    constexpr uintptr_t image=0x140000000;std::array<unsigned char,0x550> window{};std::array<std::array<unsigned char,0x4d8>,11> objects{};
    const auto put=[](auto& target,size_t offset,auto value){std::memcpy(target.data()+offset,&value,sizeof value);};
    for(size_t i=0;i<4;++i){put(window,dungeon_light_input::spinner_offsets[i],uintptr_t(objects[i].data()));put(objects[i],0,image+0x16c1720);put(objects[i],0x44c,input.spinners[i]);
        put(window,dungeon_light_input::color_offsets[i],uintptr_t(objects[7+i].data()));put(objects[7+i],0,image+0x17224a0);put(objects[7+i],0x2c8,input.colors[i]);}
    for(size_t i=0;i<3;++i){put(window,dungeon_light_input::slider_offsets[i],uintptr_t(objects[4+i].data()));put(objects[4+i],0,image+0x1787a30);put(objects[4+i],0x4d0,input.sliders[i]);}
    auto read=[](uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;};DungeonLightInput actual;dungeon_light_input::Widgets actual_widgets;
    check(dungeon_light_input::read(read,image,uintptr_t(window.data()),actual,actual_widgets,error)&&actual==input,"Native input widget/raw record layout differs");
    for(auto field:{size_t(0x428),size_t(0x430)}){put(objects[0],field,uintptr_t(1));check(!dungeon_light_input::read(read,image,uintptr_t(window.data()),actual,actual_widgets,error),"Spinner gameplay binding accepted");put(objects[0],field,uintptr_t(0));}
    for(auto field:{size_t(0x420),size_t(0x428)}){put(objects[4],field,uintptr_t(1));check(!dungeon_light_input::read(read,image,uintptr_t(window.data()),actual,actual_widgets,error),"Slider gameplay binding accepted");put(objects[4],field,uintptr_t(0));}
    put(objects[7],0x370,uintptr_t(1));check(!dungeon_light_input::read(read,image,uintptr_t(window.data()),actual,actual_widgets,error),"Color gameplay binding accepted");put(objects[7],0x370,uintptr_t(0));
    put(window,dungeon_light_input::spinner_offsets[1],uintptr_t(objects[0].data()));check(!dungeon_light_input::read(read,image,uintptr_t(window.data()),actual,actual_widgets,error)&&error.find("uniqueness")!=std::string::npos,"Aliased native widgets accepted");put(window,dungeon_light_input::spinner_offsets[1],uintptr_t(objects[1].data()));
    put(objects[0],0,image+0x1787a30);check(!dungeon_light_input::read(read,image,uintptr_t(window.data()),actual,actual_widgets,error)&&error.find("address point")!=std::string::npos,"Wrong native widget class accepted");put(objects[0],0,image+0x16c1720);
    auto failed_read=[](uintptr_t,size_t,auto&,std::string& e){e="original read cause";return false;};check(!dungeon_light_input::read(failed_read,image,uintptr_t(window.data()),actual,actual_widgets,error)&&error=="original read cause","Original widget read failure lost its cause");
    std::cout<<checks<<" Dungeon light full-record/primitive-map/input-binding/protocol checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
