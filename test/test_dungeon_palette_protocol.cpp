#include "test_dungeon_palette_fixture.h"
#include <iostream>
#include <limits>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(checks));};
    for(auto edit:{DungeonPaletteEdit::Colors,DungeonPaletteEdit::Options})for(bool defaults:{false,true}){if(edit==DungeonPaletteEdit::Colors&&defaults)continue;palette_edit=edit;default_palette=defaults;
        for(bool equal:{false,true}){equal_apply=equal;for(const auto& message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Dungeon palette complete schema35 roundtrip differs");}
            const auto invalid=[&](auto mutate){auto m=result();mutate(m);check(!encode(m,bytes,error),"Foreign Dungeon palette effect/domain accepted");};
            invalid([](auto& m){m.request.dungeon_palette.key="foreign";});invalid([](auto& m){m.request.dungeon_palette.before_sha256="bad";});invalid([](auto& m){m.request.dungeon_palette.edit=DungeonPaletteEdit(2);});
            invalid([](auto& m){m.request.dungeon_palette.colors[0][0]=std::numeric_limits<float>::infinity();});invalid([](auto& m){m.evidence.dungeon_palette_after.palette[0]^=1;});
            invalid([](auto& m){m.evidence.dungeon_palette_after.palette.back()^=1;});invalid([](auto& m){m.evidence.dungeon_palette_after.defaults=!m.request.dungeon_palette.defaults;});
            invalid([](auto& m){m.evidence.dungeon_palette_after.cells[0].theme="foreign";});invalid([](auto& m){m.evidence.dungeon_palette_after.cells[1].open=true;});invalid([](auto& m){m.evidence.dungeon_palette_after.cells[0].colors[3]^=1;});
            invalid([](auto& m){m.evidence.dungeon_palette_after.cells.pop_back();});invalid([](auto& m){m.evidence.dungeon_palette_after.palette.push_back(10);});
            invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.light[0]+=1;});invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.atmosphere=5;});
            invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.dungeon.desired+=1;});invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.dungeon.price+=1;});
            invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.dungeon.instances[0].started=false;});invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.dungeon.undo_count+=1;});
            invalid([](auto& m){m.evidence.dungeon_palette_after.protected_state.dungeon.global_random[0]+=1;});invalid([](auto& m){m.evidence.after.cash+=1;});invalid([](auto& m){m.evidence.after.line_counts[0]+=1;});
            invalid([](auto& m){m.request.dungeon_sound.key=m.request.dungeon_palette.key;});invalid([](auto& m){m.evidence.dungeon_sound_after.atmosphere=1;});
            check(dungeon_palette_wire_fits(request().request,before(),money(),error),"Normal complete palette receipt rejected");
            auto oversized=before();for(size_t i=0;i<max_payload/100;++i)oversized.cells.push_back({std::string(100,'a'),{1,2,3,4},true});check(!dungeon_palette_wire_fits(request().request,oversized,money(),error),"Oversized cell witness admitted before palette effect");
            auto json=nlohmann::json::parse([&](){need(encode(result(),bytes,error),error);return bytes;}());json["evidence"]["dungeon_palette_after"]["palette"][0]=uint64_t(UINT32_MAX)+1;auto malformed=json.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Oversized packed native color accepted");
            need(encode(result(),bytes,error),error);json=nlohmann::json::parse(bytes);json["request"]["dungeon_palette"]["defaults"]=1;malformed=json.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Non-boolean native palette flag accepted");
            need(encode(result(),bytes,error),error);json=nlohmann::json::parse(bytes);json["evidence"]["dungeon_palette_after"]["cells"][0]["colors"][0]=1.25;malformed=json.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Fractional packed cell color accepted");
        }
    }
    palette_edit=DungeonPaletteEdit::Colors;default_palette=true;check(!encode(request(),bytes,error),"UpdateColors admitted a true default flag");
    std::cout<<checks<<" Dungeon palette complete cell/palette/config/protocol comparisons passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
