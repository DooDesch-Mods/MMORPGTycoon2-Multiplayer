#include "ability_edit_fixture.h"
#include <iostream>
#include <limits>
int main()try{size_t checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const char* cause){++checks;need(ok,cause);};
    for(int32_t kind=0;kind<5;++kind)for(bool edited:{false,true}){character_kind=kind;edited_before=edited;
        for(auto message:{request(),result()}){check(encode(message,bytes,error),error.c_str());check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==message,"Ability complete authored record schema35 roundtrip differs");}
        const auto invalid=[&](auto mutate){auto message=result();mutate(message);check(!encode(message,bytes,error),"Ability accepted foreign record effect/domain");};
        invalid([](auto& m){m.request.ability_edit.kind=5;});invalid([](auto& m){m.request.ability_edit.id=-1;});invalid([](auto& m){m.request.ability_edit.slot=-1;});
        invalid([](auto& m){m.request.canonical_record_key="character:1:1/ability/3";});invalid([](auto& m){m.request.ability_edit.before_sha256="not a digest";});
        invalid([](auto& m){m.request.ability_edit.record.effects.clear();});invalid([](auto& m){m.request.ability_edit.record.effects[0].integers[0]=8;});
        invalid([](auto& m){m.request.ability_edit.record.effects[0].values[0]=std::numeric_limits<float>::infinity();});
        invalid([](auto& m){m.request.ability_edit.record.text[0]=std::string("a\0b",3);});invalid([](auto& m){m.request.ability_edit.record.costs.resize(65);});
        invalid([](auto& m){m.evidence.ability_edit_after.record.text[7]="Lost description";});invalid([](auto& m){m.evidence.ability_edit_after.record.costs[0].values[0]+=1;});
        invalid([](auto& m){++m.evidence.ability_edit_after.count;});invalid([](auto& m){++m.evidence.ability_edit_after.owner.undo_count;});
        invalid([](auto& m){m.evidence.ability_edit_after.owner.name="Wrong owner";});invalid([](auto& m){m.evidence.ability_edit_after.owner.speed+=1;});
        invalid([](auto& m){++m.evidence.ability_edit_after.owner.boss_health;});invalid([](auto& m){++m.evidence.ability_edit_after.owner.boss_phases;});
        invalid([](auto& m){++m.evidence.ability_edit_after.owner.release_edits[0];});invalid([](auto& m){++m.evidence.after.cash;});invalid([](auto& m){m.request.character_rename.id=1;});
        invalid([](auto& m){m.evidence.character_rename_before.name="foreign";});
        invalid([](auto& m){m.evidence.ability_edit_after.all_sha256=m.evidence.ability_edit_before.all_sha256;});
        auto equal=result();equal.request.ability_edit.record=equal.evidence.ability_edit_before.record;equal.evidence.ability_edit_after.record=equal.request.ability_edit.record;
        equal.evidence.ability_edit_after.all_sha256=equal.evidence.ability_edit_before.all_sha256;
        check(encode(equal,bytes,error),"Original Apply with equal authored record after Simplify lost its metadata transaction");
        auto message=request();check(encode(message,bytes,error),error.c_str());auto encoded=nlohmann::json::parse(bytes);encoded["request"]["ability_edit"]["record"]["flags"][0]=1;
        auto malformed=encoded.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Ability bool accepted integer");
        encoded=nlohmann::json::parse(bytes);encoded["request"]["ability_edit"]["record"]["selectors"].push_back(0);malformed=encoded.dump();check(!decode(reinterpret_cast<const uint8_t*>(malformed.data()),malformed.size(),decoded,error),"Ability fixed ABI record dimension accepted extra field");
        check(ability_edit_wire_fits(request().request,before(),{},error),"Normal Ability receipt failed packet preflight");
        auto oversized=request().request;oversized.ability_edit.record.text.fill(std::string(4096,'x'));
        check(!ability_edit_wire_fits(oversized,before(),{},error),"Ability oversized receipt was allowed before original effect");
    }
    std::cout<<checks<<" Ability authored record/protocol/owner/Undo checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
