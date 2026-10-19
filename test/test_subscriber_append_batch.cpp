#include "subscriber_lifecycle/additions.h"
#include "mt2mp/core/hash.h"
#include <iostream>
#include <stdexcept>

using Json=nlohmann::json;
namespace additions=mt2mp::subscriber_lifecycle::additions;
namespace native=mt2mp::subscriber_lifecycle::first_append;
unsigned checks=0;
void require(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
const std::string party_key="action:aaaaaaaa-bbbb-cccc-dddd-123456789abc:party:0";
std::string digest(const Json& value){const auto bytes=value.dump();return mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));}
Json payload(int32_t id,int32_t peer,int32_t next=3){
    Json records=Json::array();std::map<size_t,size_t> children;
    const auto add=[&](size_t parent,const char* name,Json tokens){
        const auto index=records.size();records.push_back({{"parent",parent==SIZE_MAX?Json(nullptr):Json(parent)},
            {"child_index",parent==SIZE_MAX?0:children[parent]++},{"label",{{"type",1},{"text",name}}},{"tokens",tokens}});return index;
    };
    const auto integer=[](uint32_t bits){return Json::array({{{"type",4},{"bits",bits}}});};
    add(SIZE_MAX,"mmoSubscriber",Json::array());add(0,"id",integer(uint32_t(id)));
    add(0,"linkId",integer(1));auto toon=add(0,"toon",Json::array());toon=add(toon,"mmoToon",Json::array());
    add(toon,"linkId",integer(2));add(toon,"party",integer(3));if(peer>=0)add(toon,"peer",integer(4));
    auto brain=add(0,"brain",Json::array());brain=add(brain,"mmoToonBrainAI",Json::array());brain=add(brain,"needs",Json::array());
    for(const auto* name:{"Health","Interact","Advance","Danger","GetLoot","Scenery","InventorySpace"}){
        auto need=add(brain,"need",Json::array());add(need,name,Json::array({{{"type",3},{"bits",uint32_t(0)}}}));
    }
    Json rows=Json::array({{{"saved_link_id",1},{"inline_definition",true}},{{"saved_link_id",2},{"inline_definition",true}}});
    Json owner={{"kind","shared_party"},{"party_key",party_key},{"region_id",-1},{"character_kind",-1},
        {"character_id",-1},{"ability_index",-1},{"source_snapshot_sha256",std::string(64,'b')}};
    Json external=Json::array({{{"saved_link_id",3},{"inline_definition",false},{"class_addresspoint_rva",uint32_t(0x1744e90)},{"portable_owner",owner}}});
    if(peer>=0){owner.erase("party_key");owner["kind"]="subscriber_toon";owner["subscriber_id"]=peer;
        external.push_back({{"saved_link_id",4},{"inline_definition",false},{"class_addresspoint_rva",uint32_t(0x1744e90)},{"portable_owner",owner}});}
    for(const auto& row:external)rows.push_back(row);
    return {{"schema",1},{"origin_sha256",std::string(64,'b')},{"source_record_sha256",digest(records)},
        {"references_sha256",digest(rows)},{"external_references_sha256",digest(external)},
        {"serialized_subscriber_id",id},{"next_subscriber_id",next},{"online",true},{"records",records},{"references",rows}};
}
additions::State state(bool cycle=true,int32_t first_peer=2,int32_t next=3){
    additions::State value;value.available=true;value.next_id=next;value.baseline={0};std::string error;
    for(const auto& graph:{payload(1,cycle?first_peer:-1,next),payload(2,1,next)}){
        std::string wire;if(!additions::pack(graph,wire,error))throw std::runtime_error("fixture pack: "+error);value.graphs.push_back(wire);
    }
    if(!additions::valid(value,error))throw std::runtime_error("fixture state: "+error);return value;
}
Json confirmed(const std::vector<Json>& graphs){Json rows=Json::array();for(const auto& graph:graphs)rows.push_back({{"id",graph.at("serialized_subscriber_id")},{"state","confirmed"}});return {{"confirmed",true},{"unknown",false},{"rows",rows}};}
int main()try{
    const auto cyclic=state();std::string error;Json receipt;
    {
        additions::Cache cache;unsigned batches=0,legacy=0;std::set<int32_t> available={0};
        const additions::Cache::Append append=[&](const Json&,Json&,std::string&){++legacy;return false;};
        const additions::Cache::AppendBatch batch=[&](const std::vector<Json>& graphs,Json& out,std::string& e){
            ++batches;require(graphs.size()==2,"one call receives every missing constructor");
            require(graphs[0].at("serialized_subscriber_id")==2&&graphs[1].at("serialized_subscriber_id")==1,"native batch preserves target order rather than graph order");
            for(const auto& graph:graphs)available.insert(graph.at("serialized_subscriber_id").get<int32_t>());
            unsigned party_refs=0,toon_refs=0;
            for(const auto& graph:graphs){native::Payload p;require(native::parse_payload(graph,p,e),"callback receives complete original payload: "+e);
                for(const auto& ref:p.external){if(ref.key.kind==native::ReferenceKind::SubscriberToon){++toon_refs;require(available.contains(ref.key.subscriber_id),"all new Toon owners exist before reciprocal dependency resolution");}
                    if(ref.key.kind==native::ReferenceKind::SharedParty){++party_refs;require(ref.key.party_key==party_key,"same pending Party canonical identity survives whole batch");}}
            }
            require(party_refs==2&&toon_refs==2,"both cyclic Toon and shared Party edges reach native callback");out=confirmed(graphs);return true;
        };
        require(cache.client({0},{0,2,1},cyclic,append,receipt,error,{},batch),"whole cyclic batch accepted: "+error);
        require(batches==1&&legacy==0,"optional batch replaces per-row callback exactly once");
        require(receipt.at("created_ids")==Json::array({2,1})&&receipt.at("confirmed")==true,"complete receipt commits original target order");
        require(cache.status().at("known_additions")==2&&!cache.locked(),"only complete confirmed batch commits cache");
        const additions::Cache::AppendBatch empty=[&](const std::vector<Json>& graphs,Json& out,std::string&){++batches;require(graphs.empty(),"confirmed replay never reconstructs a Subscriber");out=confirmed(graphs);return true;};
        require(cache.client({0,2,1},{0,2,1},cyclic,append,receipt,error,{},empty)&&batches==2,"empty missing batch still runs Party current-state callback once");
    }
    {
        additions::Cache cache;unsigned calls=0;
        const auto batch=[&](const std::vector<Json>&,Json&,std::string&){++calls;return true;};
        require(!cache.client({0},{0,1,2},state(true,3,4),{},receipt,error,{},batch),"reference outside current and complete new cohort rejected before mutation");
        require(calls==0&&!cache.locked()&&cache.status().at("known_additions")==0,"failed dependency preflight neither calls nor commits native batch");
    }
    const std::vector<Json> bad_receipts={
        {{"confirmed",true},{"rows",Json::array()}},
        {{"rows",Json::array({{{"id",1},{"state","confirmed"}}})}},
        {{"rows",Json::array({{{"id",1},{"state","unknown"}}})}},
        {{"attempted",true},{"confirmed",false},{"rows",Json::array()}},
        {{"unknown",true},{"confirmed",false},{"rows",Json::array()}},
        {{"confirmed",true},{"rows",Json::array({{{"id",2},{"state","confirmed"}},{{"id",1},{"state","confirmed"}}})}},
        {{"confirmed",true},{"rows",Json::array({{{"id",1},{"state","confirmed"}},{{"id",2},{"state","unknown"}}})}},
        {{"confirmed",true},{"rows",Json::array({{{"id",1},{"state","confirmed"}},{{"id",1},{"state","confirmed"}}})}}
    };
    for(const auto& native_receipt:bad_receipts){
        additions::Cache cache;unsigned calls=0;
        const auto batch=[&](const std::vector<Json>&,Json& out,std::string&){++calls;out=native_receipt;return true;};
        require(!cache.client({0},{0,1,2},cyclic,{},receipt,error,{},batch),"partial, malformed or unknown effect cannot confirm whole batch");
        require(cache.locked()&&receipt.at("unknown")==true,"any confirmed/unknown native effect locks replay even without aggregate attempted flag");
        require(cache.status().at("known_additions")==0&&receipt.at("created_ids").empty(),"row validation is atomic with respect to cache commit");
        require(!cache.client({0},{0,1,2},cyclic,{},receipt,error,{},batch)&&calls==1,"uncertain effects cannot replay original constructors");
    }
    {
        additions::Cache cache;const auto batch=[](const std::vector<Json>&,Json& out,std::string& e){out={{"attempted",true}};e="original Party prepare failed";return false;};
        require(!cache.client({0},{0,1,2},cyclic,{},receipt,error,{},batch)&&error=="original Party prepare failed","original native failure cause retained");
        require(cache.locked()&&cache.status().at("known_additions")==0,"failed attempted native call forbids commit");
    }
    {
        additions::Cache cache;unsigned calls=0;const auto batch=[&](const std::vector<Json>&,Json&,std::string&)->bool{++calls;throw std::runtime_error("original Resolve exception");};
        require(!cache.client({0},{0,1,2},cyclic,{},receipt,error,{},batch)&&error.find("original Resolve exception")!=std::string::npos,"original exception remains visible");
        require(cache.locked()&&cache.status().at("known_additions")==0,"exception cannot commit cache");
        require(!cache.client({0},{0,1,2},cyclic,{},receipt,error,{},batch)&&calls==1,"exception cannot replay constructors");
    }
    {
        additions::Cache cache;const auto rejected=[](const std::vector<Json>&,Json& out,std::string&){out={{"attempted",false},{"confirmed",false},{"unknown",false}};return false;};
        require(!cache.client({0},{0,1,2},cyclic,{},receipt,error,{},rejected)&&!cache.locked(),"proven never-attempted rejection does not lock cache");
        const auto retry=[](const std::vector<Json>& graphs,Json& out,std::string&){out=confirmed(graphs);return true;};
        require(cache.client({0},{0,1,2},cyclic,{},receipt,error,{},retry),"never-attempted preflight permits later confirmed construction");
    }
    {
        additions::Cache cache;unsigned calls=0;const auto append=[&](const Json&,Json& out,std::string&){++calls;out={{"state","confirmed"}};return true;};
        require(!cache.client({0},{0,1,2},cyclic,append,receipt,error)&&calls==0&&!cache.locked(),"legacy sequential path still rejects forward cycles before effects");
        require(cache.client({0},{0,1,2},state(false),append,receipt,error)&&calls==2,"legacy dependency-ordered path still appends each original once");
        require(cache.status().at("known_additions")==2&&receipt.at("created_ids")==Json::array({1,2}),"legacy confirmed additions preserve cache receipts");
    }
    {
        additions::Cache cache;unsigned calls=0;const auto append=[&](const Json&,Json& out,std::string&){++calls;out={{"state",calls==1?"confirmed":"unknown"}};return calls==1;};
        require(!cache.client({0},{0,1,2},state(false),append,receipt,error)&&calls==2,"legacy partial native failure remains observable");
        require(cache.locked()&&cache.status().at("known_additions")==1&&receipt.at("created_ids")==Json::array({1}),"legacy already-confirmed first row remains committed");
        require(!cache.client({0,1},{0,1,2},state(false),append,receipt,error)&&calls==2,"legacy unknown remainder cannot replay");
    }
    std::cout<<checks<<" Subscriber append batch checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
