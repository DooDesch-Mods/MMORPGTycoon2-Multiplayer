#include "test_character_membership_fixture.h"
#include <iostream>
#include <memory>
int main()try{unsigned checks=0;std::string error;std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();
    const auto check=[&](bool ok,const std::string& why){++checks;need(ok,why+" cause="+error);};
    for(const auto command:{CharacterMembershipCommand::CreateVariant,CharacterMembershipCommand::Destroy,CharacterMembershipCommand::Import}){
        auto value=std::make_unique<Message>(result(command));
        check(encode(*value,bytes,error),"Complete current Character membership rejected");
        check(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*value,"Complete Character membership input/effect roundtrip changed");
        const auto json=nlohmann::json::parse(bytes);
        check(json.at("schema")==uint64_t(command==CharacterMembershipCommand::Import?37:35),"Existing Character membership schema compatibility changed");
        if(command!=CharacterMembershipCommand::Import)continue;
        check(character_membership_wire_fits(value->request,value->evidence.character_membership_before,money(),error),"Import preflight rejected complete bounded evidence");
        const auto invalid=[&](auto mutate){auto changed=std::make_unique<Message>(*value);mutate(*changed);check(!encode(*changed,bytes,error),"Malformed/foreign import accepted");};
        invalid([](auto& m){m.request.character_membership.compressed_record.clear();});
        invalid([](auto& m){m.request.character_membership.compressed_record="Ff";});
        invalid([](auto& m){m.request.character_membership.compressed_record="abc";});
        invalid([](auto& m){m.request.character_membership.name="foreign";});
        invalid([](auto& m){m.request.character_membership.compressed_record=std::string(60002,'0');});
        invalid([](auto& m){m.evidence.character_membership_after.counts[1]++;});
        invalid([](auto& m){m.evidence.character_membership_after.selected.parent=2;});
        invalid([](auto& m){m.evidence.character_membership_after.selected.deleted=true;});
        invalid([](auto& m){m.evidence.character_membership_after.selected.variants.pop_back();});
        invalid([](auto& m){m.evidence.character_membership_after.others_sha=std::string(64,'f');});
        invalid([](auto& m){m.evidence.character_membership_after.import_art_sha.clear();});
        invalid([](auto& m){m.evidence.character_membership_before.import_art_sha="bad";});
        invalid([](auto& m){m.evidence.character_membership_after.import_release_sha.clear();});
        invalid([](auto& m){m.evidence.character_membership_after.undo_count++;});
        invalid([](auto& m){m.evidence.after.cash++;});
        auto same=std::make_unique<Message>(*value);same->evidence.character_membership_after=same->evidence.character_membership_before;
        check(encode(*same,bytes,error),"Importing the same original contents invented a required mutation");
        for(const auto schema:{35u,36u,38u}){auto wrong=json;wrong["schema"]=schema;const auto text=wrong.dump();
            check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Import escaped its exact schema admission");}
        auto wrong=json;wrong["request"]["character_membership"]["native_image"]=123u;auto text=wrong.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Raw image pointer field admitted");
        wrong=json;wrong["request"]["character_membership"]["command"]=0u;text=wrong.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Legacy command admitted through new Import schema");
        wrong=json;wrong["evidence"]["character_membership_after"].erase("import_art_sha");text=wrong.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Incomplete original ArtStore effect proof admitted");
    }
    std::cout<<checks<<" actual Character Import87 codec/full effect/legacy schema checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
