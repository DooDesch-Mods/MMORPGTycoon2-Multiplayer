#include "payload.h"
#include "record_decode.h"
#include "mt2mp/core/hash.h"
#include <bit>
#include <cmath>
#include <set>
namespace mt2mp::subscriber_lifecycle::first_append {
namespace {
std::string digest(const nlohmann::json& j){const auto s=j.dump();return mt2mp::sha256(std::vector<unsigned char>(s.begin(),s.end()));}
int32_t integer(const nlohmann::json& j){if(!j.is_number_integer()||j.is_boolean()||j.get<int64_t>()<INT32_MIN||j.get<int64_t>()>INT32_MAX)throw std::runtime_error("Subscriber payload integer outside int32");return j.get<int32_t>();}
Token token(const nlohmann::json& j){Token t;t.type=integer(j.at("type"));if(t.type==1||t.type==2)t.text=j.at("text").get<std::string>();else if(t.type){const auto& v=j.at("bits");if(!v.is_number_unsigned()||v.get<uint64_t>()>UINT32_MAX)throw std::runtime_error("Subscriber payload token bits outside uint32");t.bits=v.get<uint32_t>();}return t;}
}
bool parse_payload(const nlohmann::json& j,Payload& out,std::string& e){
    out={};try{
        if(integer(j.at("schema"))!=1)throw std::runtime_error("Subscriber payload schema differs");
        out.baseline_sha=j.at("origin_sha256").get<std::string>();out.record_sha=j.at("source_record_sha256").get<std::string>();out.references_sha=j.at("references_sha256").get<std::string>();
        if(out.baseline_sha.size()!=64||digest(j.at("records"))!=out.record_sha||digest(j.at("references"))!=out.references_sha)throw std::runtime_error("Subscriber payload record/reference digest differs");
        out.id=integer(j.at("serialized_subscriber_id"));out.next_id=integer(j.at("next_subscriber_id"));out.online=j.at("online").get<bool>();
        if(out.id<0||out.next_id<=out.id)throw std::runtime_error("Subscriber payload actual ID/nextID invalid");
        for(const auto& row:j.at("records")){Record r;const auto& parent=row.at("parent");r.parent=parent.is_null()?SIZE_MAX:parent.get<size_t>();r.child_index=row.at("child_index").get<size_t>();r.label=token(row.at("label"));for(const auto& t:row.at("tokens"))r.tokens.push_back(token(t));out.records.push_back(std::move(r));}
        if(!validate_records(out.records,e))return false;
        std::set<int32_t> definitions,links;int id_fields=0;
        for(const auto& r:out.records){if(r.parent==0&&r.label.text=="id"){if(r.tokens.size()!=1||r.tokens[0].type!=4||std::bit_cast<int32_t>(r.tokens[0].bits)!=out.id)throw std::runtime_error("Subscriber root ID differs");++id_fields;}
            if(r.label.text=="linkId"){if(r.tokens.size()!=1||r.tokens[0].type!=4||r.tokens[0].bits==0||r.tokens[0].bits>INT32_MAX||!definitions.insert(static_cast<int32_t>(r.tokens[0].bits)).second)throw std::runtime_error("Subscriber inline link definition invalid");}}
        if(id_fields!=1)throw std::runtime_error("Subscriber root ID not unique");
        const std::array<std::string,7> names={"Health","Interact","Advance","Danger","GetLoot","Scenery","InventorySpace"};
        std::array<bool,7> found{};
        std::vector<std::string> paths;
        for(const auto& r:out.records){
            paths.push_back(r.parent==SIZE_MAX?r.label.text:paths.at(r.parent)+"/"+r.label.text);
            for(size_t i=0;i<names.size();++i)if(paths.back()=="mmoSubscriber/brain/mmoToonBrainAI/needs/need/"+names[i]){
                if(found[i]||r.tokens.size()!=1||r.tokens[0].type!=3||!std::isfinite(std::bit_cast<float>(r.tokens[0].bits)))throw std::runtime_error("Subscriber captured Need missing/duplicate/nonfinite");
                found[i]=true;out.needs_bits[i]=r.tokens[0].bits;
            }
        }
        for(bool present:found)if(!present)throw std::runtime_error("Subscriber captured Need absent");
        nlohmann::json external=nlohmann::json::array();
        for(const auto& row:j.at("references")){
            const auto id=integer(row.at("saved_link_id"));if(id<=0||!links.insert(id).second)throw std::runtime_error("Subscriber reference IDs invalid/duplicate");
            if(row.at("inline_definition").get<bool>()){if(!definitions.erase(id))throw std::runtime_error("Subscriber inline definition/reference differs");out.local_ids.push_back(id);continue;}
            external.push_back(row);const auto& key=row.at("portable_owner");if(key.at("source_snapshot_sha256").get<std::string>()!=out.baseline_sha)throw std::runtime_error("Subscriber external owner baseline differs");
            ExternalReference ref;ref.local_link_id=id;const auto kind=key.at("kind").get<std::string>();
            if(kind=="root_shard")ref.key.kind=ReferenceKind::RootShard;else if(kind=="root_region")ref.key.kind=ReferenceKind::RootRegion;else if(kind=="character_ability")ref.key.kind=ReferenceKind::CharacterAbility;else if(kind=="shared_building")ref.key.kind=ReferenceKind::SharedBuilding;else if(kind=="subscriber_toon")ref.key.kind=ReferenceKind::SubscriberToon;else if(kind=="shared_npc")ref.key.kind=ReferenceKind::SharedNPC;else if(kind=="shared_quest")ref.key.kind=ReferenceKind::SharedQuest;else if(kind=="shared_party")ref.key.kind=ReferenceKind::SharedParty;else throw std::runtime_error("Subscriber external owner kind unsupported");
            ref.key.region_id=integer(key.at("region_id"));ref.key.character_kind=integer(key.at("character_kind"));ref.key.character_id=integer(key.at("character_id"));ref.key.ability_index=integer(key.at("ability_index"));if(ref.key.kind==ReferenceKind::SharedBuilding){
                ref.key.building_key=key.at("building_key").get<std::string>();
                if(!valid_building_key(ref.key.building_key)||ref.key.region_id!=-1||ref.key.character_kind!=-1||ref.key.character_id!=-1||ref.key.ability_index!=-1)throw std::runtime_error("Subscriber Building key invalid/foreign fields");
            }else if(key.contains("building_key"))throw std::runtime_error("Subscriber non-Building owner has Building key");
            if(ref.key.kind==ReferenceKind::SharedNPC||ref.key.kind==ReferenceKind::SharedQuest){
                ref.key.npc_key=key.at("npc_key").get<std::string>();
                if(!valid_npc_key(ref.key.npc_key)||ref.key.region_id!=-1||ref.key.character_kind!=-1||ref.key.character_id!=-1||ref.key.ability_index!=-1)throw std::runtime_error("Subscriber NPC key invalid/foreign fields");
            }else if(key.contains("npc_key"))throw std::runtime_error("Subscriber non-NPC owner has NPC key");
            if(ref.key.kind==ReferenceKind::SharedQuest){
                ref.key.sequence_id=integer(key.at("sequence_id"));
                if(ref.key.sequence_id<0)throw std::runtime_error("Subscriber Quest sequence invalid");
            }else if(key.contains("sequence_id"))throw std::runtime_error("Subscriber non-Quest owner has Quest sequence");
            if(ref.key.kind==ReferenceKind::SubscriberToon){
                ref.key.subscriber_id=integer(key.at("subscriber_id"));
                if(ref.key.subscriber_id<0||ref.key.subscriber_id>=out.next_id||ref.key.subscriber_id==out.id||ref.key.region_id!=-1||ref.key.character_kind!=-1||ref.key.character_id!=-1||ref.key.ability_index!=-1)throw std::runtime_error("Subscriber Toon key invalid/self/foreign fields");
            }else if(key.contains("subscriber_id"))throw std::runtime_error("Subscriber non-Toon owner has Subscriber ID");
            if(ref.key.kind==ReferenceKind::SharedParty){
                ref.key.party_key=key.at("party_key").get<std::string>();
                if(!valid_party_key(ref.key.party_key)||ref.key.region_id!=-1||ref.key.character_kind!=-1||ref.key.character_id!=-1||ref.key.ability_index!=-1)
                    throw std::runtime_error("Subscriber Party key invalid/foreign fields");
            }else if(key.contains("party_key"))throw std::runtime_error("Subscriber non-Party owner has Party key");
            out.external.push_back(ref);
        }
        if(!definitions.empty()||digest(external)!=j.at("external_references_sha256").get<std::string>())throw std::runtime_error("Subscriber external closure digest/definitions differ");
        e.clear();return true;
    }catch(const std::exception& ex){e=ex.what();return false;}
}
}
