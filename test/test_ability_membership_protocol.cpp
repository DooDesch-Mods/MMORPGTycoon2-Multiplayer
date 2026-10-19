#include "test_ability_membership_fixture.h"
#include <iostream>
int main()try{
    unsigned checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const std::string& why){++checks;need(ok,why);};
    for(const auto command:{AbilityMembershipCommand::Create,AbilityMembershipCommand::Remove,AbilityMembershipCommand::Dock,AbilityMembershipCommand::Copy}){
        auto value=result(command);check(encode(value,bytes,error),error);
        check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==value,"Complete native Ability command/effects did not roundtrip");
        check(ability_membership_wire_fits(value.request,value.evidence.ability_membership_before,money(),error),error);
        const auto invalid=[&](auto mutate){auto changed=value;mutate(changed);check(!encode(changed,bytes,error),"Foreign/incomplete Ability authoring accepted");};
        invalid([](auto& m){m.evidence.retirement_confirmed=false;});invalid([](auto& m){m.request.ability_membership.command=AbilityMembershipCommand(4);});
        invalid([](auto& m){m.request.ability_membership.kind=5;});invalid([](auto& m){m.request.ability_membership.before_sha256="bad";});
        invalid([](auto& m){m.request.canonical_record_key="character:1:1/abilities";});invalid([](auto& m){m.request.target_action_uuid=action_id;});
        invalid([](auto& m){m.evidence.ability_membership_after.complete.counts[0]++;});
        invalid([](auto& m){m.evidence.ability_membership_after.complete.selected.name="foreign";});
        invalid([](auto& m){m.evidence.ability_membership_after.complete.retired_definitions={0};});
        invalid([](auto& m){m.evidence.ability_membership_after.complete.undo_count++;});
        invalid([](auto& m){m.evidence.ability_membership_after.complete.users_sha="bad";});
        invalid([](auto& m){m.evidence.after.cash++;});invalid([](auto& m){m.evidence.undo_confirmed=true;});
        if(command==AbilityMembershipCommand::Create){
            invalid([](auto& m){m.evidence.ability_membership_after.slots.back().record.level++;});
            invalid([](auto& m){m.evidence.ability_membership_after.slots.back().display_slot=0;});
            invalid([](auto& m){m.evidence.ability_membership_after.slots.front().record.text[0]="other";});
            auto full=value;full.evidence.ability_membership_before.slots.resize(12,{record(1),0});
            full.evidence.ability_membership_after=full.evidence.ability_membership_before;full.evidence.ability_membership_after.slots.push_back({record(9),-1});
            check(!encode(full,bytes,error),"Native NewAbility12 limit was dropped");
        }else if(command==AbilityMembershipCommand::Remove){
            invalid([](auto& m){m.evidence.ability_membership_after.slots.clear();});
            invalid([](auto& m){m.evidence.ability_membership_after.slots.front().record.effects.front().values[0]++;});
        }else if(command==AbilityMembershipCommand::Dock){
            invalid([](auto& m){m.evidence.ability_membership_after.slots[1].display_slot++;});
            invalid([](auto& m){m.evidence.ability_membership_after.slots[1].record.level++;});
            invalid([](auto& m){m.evidence.ability_membership_after.slots[0].record.level++;});
        }else {
            invalid([](auto& m){m.evidence.ability_membership_after.slots.back().record.text[0]="guessed name";});
            invalid([](auto& m){m.evidence.ability_membership_after.slots.back().display_slot=m.request.ability_membership.display_slot;});
            invalid([](auto& m){m.evidence.ability_membership_after.slots.back().record.effects[0].integers[1]++;});
            invalid([](auto& m){m.request.ability_membership.copy_name.clear();});
        }
        check(encode(value,bytes,error),error);auto malformed=nlohmann::json::parse(bytes);
        malformed["request"]["ability_membership"]["slot"]=1.5;auto text=malformed.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Fractional native Ability slot accepted");
        malformed=nlohmann::json::parse(bytes);malformed["request"]["ability_membership"]["prototype_pointer"]=123u;text=malformed.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Native Ability pointer field accepted");
        malformed=nlohmann::json::parse(bytes);malformed["schema"]=34u;text=malformed.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Ability command accepted another schema");
    }
    auto boss=result();boss.request.ability_membership.kind=4;boss.request.canonical_record_key="character:4:0/abilities";
    boss.evidence.ability_membership_before.complete.counts={0,0,0,0,1};boss.evidence.ability_membership_before.complete.selected.kind=4;
    boss.evidence.ability_membership_after=after(boss.request.ability_membership,boss.evidence.ability_membership_before);
    check(encode(boss,bytes,error),error);boss.evidence.ability_membership_after.slots.back().record.level=9;
    check(!encode(boss,bytes,error),"Original Boss new-ability level1 override was lost");
    auto oversized=before();oversized.complete.counts[1]=250001;
    for(int32_t i=1;i<=250000;++i)oversized.complete.selected.variants.push_back(i);
    check(!ability_membership_wire_fits(request().request,oversized,money(),error),"Complete Character variant metadata escaped the pre-mutation packet bound");
    auto invalid=request(AbilityMembershipCommand::Dock);invalid.request.ability_membership.slot=200;
    check(!ability_membership_wire_fits(invalid.request,before(),money(),error),"Absent original Ability slot passed packet preflight");
    invalid=request();invalid.request.action=Action::EditCharacterMembership;
    check(!ability_membership_wire_fits(invalid.request,before(),money(),error),"Another original action entered Ability packet preflight");
    std::cout<<checks<<" complete Ability creation/removal/display-slot/prefab-copy protocol and native bounds comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
