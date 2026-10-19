#include "test_character_membership_fixture.h"
#include <iostream>
int main()try{
    unsigned checks=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const std::string& why){++checks;need(ok,why);};
    for(const auto command:{CharacterMembershipCommand::CreateVariant,CharacterMembershipCommand::Destroy})for(bool vacant:{false,true}){
        auto value=result(command,vacant);check(encode(value,bytes,error),error);check(decode(bytes.data(),bytes.size(),decoded,error)&&decoded==value,"Complete Character family evidence did not roundtrip");
        check(character_membership_wire_fits(value.request,value.evidence.character_membership_before,money(),error),error);
        const auto invalid=[&](auto mutate){auto changed=value;mutate(changed);check(!encode(changed,bytes,error),"Incomplete or foreign Character membership was accepted");};
        invalid([](auto& m){m.evidence.retirement_confirmed=false;});
        invalid([](auto& m){m.request.character_membership.command=CharacterMembershipCommand(2);});
        invalid([](auto& m){m.request.character_membership.kind=5;});invalid([](auto& m){m.request.character_membership.before_sha="bad";});
        invalid([](auto& m){m.request.canonical_record_key="character:1:1";});invalid([](auto& m){m.request.target_action_uuid=action_id;});
        invalid([](auto& m){m.evidence.character_membership_after.manager_sha=m.evidence.character_membership_before.manager_sha;});
        invalid([](auto& m){m.evidence.character_membership_after.users_sha="bad";});invalid([](auto& m){m.evidence.character_membership_after.quests_sha="bad";});
        invalid([](auto& m){m.evidence.character_membership_after.economy_sha="bad";});invalid([](auto& m){m.evidence.character_membership_after.bosses_sha="bad";});
        invalid([](auto& m){m.evidence.character_membership_after.monsters_sha="bad";});invalid([](auto& m){m.evidence.character_membership_after.counts[0]++;});
        invalid([](auto& m){m.evidence.character_membership_before.first_deleted[1]=m.evidence.character_membership_before.counts[1];});
        invalid([](auto& m){m.evidence.character_membership_after.selected.name="foreign";});invalid([](auto& m){m.evidence.undo_confirmed=true;});
        invalid([](auto& m){m.evidence.character_membership_before.retired_definitions={0};});
        if(command==CharacterMembershipCommand::CreateVariant){
            invalid([](auto& m){m.evidence.character_membership_after.created->id++;});invalid([](auto& m){m.evidence.character_membership_after.created->parent=1;});
            invalid([](auto& m){m.evidence.character_membership_after.created->deleted=true;});invalid([](auto& m){m.evidence.character_membership_after.selected.variants.pop_back();});
            invalid([](auto& m){m.evidence.after.cash++;});invalid([](auto& m){m.evidence.character_membership_after.undo_count++;});
        }else{
            invalid([](auto& m){m.evidence.character_membership_after.retired_definitions.pop_back();});
            invalid([](auto& m){m.evidence.character_membership_after.retired_definitions={3,2,0};});
            invalid([](auto& m){m.evidence.character_membership_after.selected.deleted=false;});invalid([](auto& m){m.evidence.character_membership_after.created=CharacterMembershipRow{1,1,0,false,"foreign",{}};});
        }
        check(encode(value,bytes,error),error);auto malformed=nlohmann::json::parse(bytes);malformed["request"]["character_membership"]["kind"]=1.5;
        auto text=malformed.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Fractional native Character kind accepted");
        malformed=nlohmann::json::parse(bytes);malformed["request"]["character_membership"]["native_pointer"]=123u;text=malformed.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Foreign native pointer field accepted");
        malformed=nlohmann::json::parse(bytes);malformed["schema"]=34u;text=malformed.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"Character membership accepted another action schema");
    }
    auto single=result(CharacterMembershipCommand::Destroy);single.request.character_membership.id=2;single.request.canonical_record_key="character:1:2";
    single.evidence.character_membership_before.selected={1,2,0,false,"Wolf 1",{}};single.evidence.character_membership_after=after(single.request.character_membership,single.evidence.character_membership_before);
    check(encode(single,bytes,error),error);check(single.evidence.character_membership_after.retired_definitions==std::vector<int32_t>{2},"Variant removal retired its base or sibling");
    auto current=before();current.selected.variants.clear();current.counts[1]=250001;
    for(int32_t i=1;i<=250000;++i)current.selected.variants.push_back(i);
    auto large=request(CharacterMembershipCommand::Destroy);check(!character_membership_wire_fits(large.request,current,money(),error),"Oversized native cascade admitted before mutation");
    auto foreign=result();foreign.request.action=Action::RenameCharacter;foreign.request.character_membership={};foreign.request.character_rename={1,0,"Wolf","Wolf named"};
    check(!encode(foreign,bytes,error),"Character family evidence leaked into old rename action");
    std::cout<<checks<<" Character membership schema, first-deleted-slot reuse, original family cascade and bounded evidence comparisons passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
