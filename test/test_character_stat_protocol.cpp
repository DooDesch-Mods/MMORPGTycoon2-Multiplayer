#include "character_rename_fixture.h"
#include <iostream>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const char* cause){++checks;need(ok,cause);};
    for(int32_t kind=0;kind<5;++kind)for(uint8_t property=1;property<=4;++property){character_kind=kind;
        auto message=result();auto& intent=message.request.character_rename;intent.property=property;intent.value=57.5f;intent.name.clear();message.request.action=Action(80+property);
        auto& before=message.evidence.character_rename_before;before.attributes={{{10,20,30},{20,20,30}}};before.speed=6;before.boss_health=1000;before.boss_phases=kind==4?2:0;
        auto& after=message.evidence.character_rename_after;after=before;after.all_sha256=std::string(64,'c');after.global_random[0]+=2;
        if(property==4)after.speed=intent.value;
        else if(property==1&&kind==4)after.boss_health=int32_t(intent.value);
        else {after.attributes[0][size_t(property-1)]=intent.value;after.attributes[1][size_t(property-1)]=property==1?2.f*intent.value:intent.value;}
        check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Character stat schema35 complete effect roundtrip differs");
        const auto invalid=[&](auto mutate){auto wrong=message;mutate(wrong);check(!encode(wrong,bytes,error),"Character stat accepted wrong domain/effect");};
        invalid([](auto& m){m.request.character_rename.property=5;});invalid([](auto& m){m.request.character_rename.value=-1;});
        invalid([](auto& m){m.request.character_rename.value=1000001;});invalid([](auto& m){m.request.character_rename.name="Unrelated rename";});
        invalid([](auto& m){m.request.character_rename.before="Stale owner name";});invalid([](auto& m){m.request.action=Action::RenameCharacter;});
        invalid([](auto& m){m.evidence.character_rename_after.name="Unexpected renamed type";});
        invalid([](auto& m){m.evidence.character_rename_after.definition_name="Unexpected definition";});
        invalid([](auto& m){m.evidence.character_rename_after.released=!m.evidence.character_rename_before.released;});
        invalid([](auto& m){++m.evidence.character_rename_after.boss_phases;});invalid([](auto& m){++m.evidence.character_rename_after.count;});
        invalid([](auto& m){m.evidence.character_rename_after.others_sha256=std::string(64,'f');});
        invalid([](auto& m){++m.evidence.character_rename_after.undo_count;});invalid([](auto& m){++m.evidence.character_rename_after.release_edits[0];});
        invalid([](auto& m){m.evidence.character_rename_after.release_edits[7]+=100;});invalid([](auto& m){++m.evidence.after.cash;});
        if(property==4)invalid([](auto& m){++m.evidence.character_rename_after.speed;});
        else if(property==1&&kind==4)invalid([](auto& m){++m.evidence.character_rename_after.boss_health;});
        else invalid([&](auto& m){++m.evidence.character_rename_after.attributes[0][size_t(property-1)];});
        invalid([&](auto& m){++m.evidence.character_rename_after.attributes[0][size_t(property==1?1:0)];});
    }
    std::cout<<checks<<" Character health/mana/rage/speed/Boss protocol checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
