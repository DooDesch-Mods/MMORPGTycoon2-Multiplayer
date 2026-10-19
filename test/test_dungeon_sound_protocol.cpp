#include "test_dungeon_sound_fixture.h"
#include "dungeon_sound_record.h"
#include <cstring>
#include <iostream>
#include <limits>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(checks));};
    for(int32_t row=0;row<6;++row)for(bool equal:{false,true}){selected_atmosphere=row;equal_apply=equal;
        for(const auto& message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Dungeon atmosphere complete schema35 roundtrip differs");}
        const auto invalid=[&](auto mutate){auto m=result();mutate(m);check(!encode(m,bytes,error),"Foreign Dungeon atmosphere effect/domain accepted");};
        invalid([](auto& m){m.request.dungeon_sound.key="foreign";});invalid([](auto& m){m.request.dungeon_sound.before=-1;});invalid([](auto& m){m.request.dungeon_sound.atmosphere=6;});
        invalid([](auto& m){m.evidence.dungeon_sound_after.atmosphere=(m.request.dungeon_sound.atmosphere+1)%6;});
        invalid([](auto& m){m.evidence.dungeon_sound_before.atmosphere=(m.request.dungeon_sound.before+1)%6;});
        invalid([](auto& m){m.evidence.dungeon_sound_after.light[0]+=1;});invalid([](auto& m){m.evidence.dungeon_sound_after.dungeon.desired+=1;});invalid([](auto& m){m.evidence.dungeon_sound_after.dungeon.price+=1;});
        invalid([](auto& m){m.evidence.dungeon_sound_after.dungeon.instances[0].started=false;});invalid([](auto& m){m.evidence.dungeon_sound_after.dungeon.instances[1].index=5;});invalid([](auto& m){m.evidence.dungeon_sound_after.dungeon.undo_count+=1;});
        invalid([](auto& m){m.evidence.after.cash+=1;});invalid([](auto& m){m.evidence.after.line_counts[0]+=1;});invalid([](auto& m){m.request.dungeon_light.key=m.request.dungeon_sound.key;});
        check(dungeon_sound_wire_fits(request().request,before(),money(),error),"Normal atmosphere result packet rejected");
        auto oversized=before();for(int32_t i=3;i<10000;++i)oversized.dungeon.instances.push_back({i,false});check(!dungeon_sound_wire_fits(request().request,oversized,money(),error),"Oversized atmosphere receipt admitted before effect");
        auto message=request();check(encode(message,bytes,error),error.c_str());auto json=nlohmann::json::parse(bytes);json["request"]["dungeon_sound"]["atmosphere"]=1.5;const auto malformed=json.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Fractional atmosphere row accepted");
    }
    using namespace mt2mp::game;std::array<unsigned char,0x78> record{};std::array<unsigned char,0x30> token{};
    const auto put=[](auto& target,size_t offset,auto value){std::memcpy(target.data()+offset,&value,sizeof value);};
    put(record,0x38,uintptr_t(token.data()));put(record,0x40,int32_t(1));put(record,0x44,int32_t(1));put(token,0,int32_t(4));
    auto read=[](uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;};int32_t actual=-1;
    for(int32_t row=0;row<6;++row){put(token,0x28,row);check(dungeon_sound_record::read(read,uintptr_t(record.data()),actual,error)&&actual==row,"Native integer token layout/value differs");}
    for(int32_t row:{-1,6}){put(token,0x28,row);check(!dungeon_sound_record::read(read,uintptr_t(record.data()),actual,error),"Native row outside six choices accepted");}put(token,0x28,int32_t(3));
    put(token,0,int32_t(3));check(!dungeon_sound_record::read(read,uintptr_t(record.data()),actual,error),"Non-integer native token accepted");put(token,0,int32_t(4));
    for(int32_t count:{0,2}){put(record,0x40,count);check(!dungeon_sound_record::read(read,uintptr_t(record.data()),actual,error),"Non-normal token count accepted");}put(record,0x40,int32_t(1));
    put(record,0x44,int32_t(0));check(!dungeon_sound_record::read(read,uintptr_t(record.data()),actual,error),"Invalid token capacity accepted");put(record,0x44,int32_t(1));
    auto fail_read=[](uintptr_t,size_t,auto&,std::string& e){e="original token read cause";return false;};check(!dungeon_sound_record::read(fail_read,uintptr_t(record.data()),actual,error)&&error=="original token read cause","Native token read failure lost its cause");
    for(bool preparation:{false,true}){uint32_t calls=0;bool execute=false;
        check(!dungeon_sound_record::replay(2,3,1,3,preparation,calls,execute,error)&&!calls&&!execute,"Foreign replay receiver accepted");
        check(!dungeon_sound_record::replay(1,4,1,3,preparation,calls,execute,error)&&!calls&&!execute,"Foreign replay atmosphere accepted");
        check(dungeon_sound_record::replay(1,3,1,3,preparation,calls,execute,error)&&calls==1&&execute!=preparation,"Native preparation callback did not suppress gameplay or execution did not select original");
        check(!dungeon_sound_record::replay(1,3,1,3,preparation,calls,execute,error)&&calls==1&&!execute,"Second native callback/original admitted");
    }
    std::cout<<checks<<" Dungeon atmosphere complete-record/native-token/protocol comparisons passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
