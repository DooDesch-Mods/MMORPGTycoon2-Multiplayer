#include "character_rename_fixture.h"
#include <iostream>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const char* cause){++checks;need(ok,cause);};
    for(int32_t kind=0;kind<5;++kind)for(bool edited:{false,true}){character_kind=kind;edited_before=edited;
        for(auto message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Character rename schema35 complete roundtrip differs");}
        const auto invalid=[&](auto mutate){auto message=result();mutate(message);check(!encode(message,bytes,error),"Character rename accepted foreign effect or action domain");};
        invalid([](auto& m){m.request.character_rename.kind=5;});invalid([](auto& m){m.request.character_rename.id=-1;});
        invalid([](auto& m){m.request.canonical_record_key="character:1:999";});invalid([](auto& m){m.request.character_rename.before="Stale name";});
        invalid([](auto& m){m.request.character_rename.name=m.request.character_rename.before;});
        invalid([](auto& m){m.evidence.character_rename_after.name="Wrong name";});invalid([](auto& m){m.evidence.character_rename_after.definition_name="Wrong definition";});
        invalid([](auto& m){m.evidence.character_rename_after.edited=false;});invalid([](auto& m){++m.evidence.character_rename_after.rename_day;});
        invalid([](auto& m){m.evidence.character_rename_after.released=false;});invalid([](auto& m){++m.evidence.character_rename_after.count;});
        invalid([](auto& m){m.evidence.character_rename_after.others_sha256=std::string(64,'d');});
        invalid([](auto& m){++m.evidence.character_rename_after.undo_count;});invalid([](auto& m){++m.evidence.character_rename_after.release_edits[0];});
        invalid([](auto& m){++m.evidence.character_rename_after.release_edits[7];});invalid([](auto& m){++m.evidence.after.cash;});
        invalid([](auto& m){m.evidence.undo_confirmed=true;});invalid([](auto& m){m.request.npc_quest_list.region=67;});
        auto message=result();message.request.character_rename.name="";message.evidence.character_rename_after.name="";message.evidence.character_rename_after.definition_name="";
        check(encode(message,bytes,error),"Original empty Character rename was restricted");
        message=result();message.request.character_rename.name=std::string("x\0y",3);check(!encode(message,bytes,error),"Embedded NUL Character rename accepted");
    }
    std::cout<<checks<<" Character rename protocol/census/release/Undo checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
