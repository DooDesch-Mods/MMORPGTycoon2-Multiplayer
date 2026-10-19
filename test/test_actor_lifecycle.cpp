#include "intervention_actor_lifecycle.h"
#include <iostream>
#include <stdexcept>

namespace l=mt2mp::game::interventions::actors::lifecycle;
namespace i=mt2mp::game::interventions;
namespace {
unsigned checks=0;const std::string uuid="11111111-2222-3333-4444-555555555555",sha(64,'a');
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
l::Event boss(l::Source source,l::Effect effect){return {{i::Family::Boss,sha+":world:boss:1"},effect,source,{},0};}
bool rejected(const nlohmann::json& value){try{(void)l::decode_event(value);return false;}catch(const std::exception&){return true;}}
void domain(){const auto died=boss(l::Source::BossBaseDeath,l::Effect::CharacterDie),wait=boss(l::Source::BossWait,l::Effect::OnSpawn),spawn=boss(l::Source::BossRespawn,l::Effect::OnSpawn);
    for(const auto& event:{died,wait,spawn}){need(l::valid(event),"actual Boss original lifecycle invocation rejected");need(l::decode_event(l::encode(event))==event,"actual original lifecycle value changed on wire");}
    need(l::call_return(wait.source)==0x397afa&&l::call_return(spawn.source)==0x3996cd,"Boss wait/respawn original return witnesses were collapsed");
    l::Source source;need(l::source_for(i::Family::Boss,l::Effect::OnSpawn,0x397afa,source)&&source==l::Source::BossWait,"actual original Boss wait caller was not classified");
    need(!l::source_for(i::Family::Npc,l::Effect::OnSpawn,0x397afa,source),"NPC accepted Boss native OnSpawn caller");
    need(!l::source_for(i::Family::Toon,l::Effect::CharacterDie,0x3c200d,source),"Actor24 attempted to own Economy Toon lifecycle");
    need(l::source_for(i::Family::Npc,l::Effect::CharacterDie,0x3c200d,source)&&source==l::Source::NpcBaseDeath,"NPC base Die original direct call witness rejected");
    need(!l::source_for(i::Family::Monster,l::Effect::CharacterDie,0x3c200d,source),"Monster accepted another native family Die caller");
    need(!l::source_for(i::Family::Boss,l::Effect::OnSpawn,0x397afb,source),"one-byte wrong original return witness accepted");
    need(!l::ordered_receipt(false,true,l::Source::BossBaseDeath),"original action token was invented outside the Engine execution scope");
    need(!l::ordered_receipt(true,false,l::Source::BossRespawn),"natural SaveScope respawn was mislabeled as pending original action");
    need(l::ordered_receipt(true,true,l::Source::BossBaseDeath),"actual intervention original receipt lost action deduplication");
    need(l::ordered_receipt(true,false,l::Source::BossRegenerate),"actual authored regeneration original receipt lost action deduplication");
    auto invalid=wait;invalid.effect=l::Effect::CharacterDie;need(!l::valid(invalid),"Boss original OnSpawn was accepted as inferred Die");invalid=wait;invalid.target.family=i::Family::Npc;invalid.target.key="action:"+uuid;need(!l::valid(invalid),"Boss lifecycle witness applied to NPC");
    const l::Event npc{{i::Family::Npc,"action:"+uuid},l::Effect::ClearLivePointers,l::Source::NpcDeath,uuid,1};need(l::valid(npc),"NPC actual ordered ClearLivePointers receipt rejected");
    invalid=npc;invalid.ordinal=0;need(!l::valid(invalid),"ordered original receipt lacks invocation ordinal");invalid=npc;invalid.action.clear();need(!l::valid(invalid),"natural original receipt retained an unrelated action ordinal");
    invalid=npc;invalid.action="17";need(!l::valid(invalid),"pending peer/native ID was accepted as an original Action UUID");
    for(const char* key:{"target","effect","source","action","ordinal"}){auto bad=l::encode(npc);bad.erase(key);need(rejected(bad),"partial Actor lifecycle invocation accepted");}
    auto bad=l::encode(npc);bad["local_uid"]=1;need(rejected(bad),"Actor lifecycle leaked native identity into wire grammar");
    for(const auto& value:{nlohmann::json(true),nlohmann::json(-1),nlohmann::json(1.25),nlohmann::json(uint64_t(UINT32_MAX)+1)}){bad=l::encode(npc);bad["ordinal"]=value;need(rejected(bad),"Actor lifecycle invalid integer accepted");}
}
void journal(){const l::journal::Origin origin{7,8,9,uuid};l::Host host;l::Client client;std::string error;
    need(host.bind(origin,2,error)&&client.bind(origin,error),error);
    const auto died=boss(l::Source::BossBaseDeath,l::Effect::CharacterDie),wait=boss(l::Source::BossWait,l::Effect::OnSpawn),spawn=boss(l::Source::BossRespawn,l::Effect::OnSpawn);
    for(const auto& event:{died,wait,spawn})need(l::append(host,origin,event,error),error);
    const auto batch=host.pending();need(batch.rows.size()==3&&l::valid(batch,error),error);need(l::event(batch.rows[0])==died&&l::event(batch.rows[1])==wait&&l::event(batch.rows[2])==spawn,"complete between-Frame Boss cycle lost actual ordered original invocations");
    l::Batch decoded;need(l::decode(l::encode(batch),decoded,error)&&decoded==batch,error);
    need(host.sent(origin,1,batch,error),error);std::vector<l::journal::Row> work;
    need(client.prepare(origin,batch,work,error)&&work==batch.rows,error);need(client.confirm(origin,batch,work,error),error);
    work.clear();need(client.prepare(origin,batch,work,error)&&work.empty(),"repeated Frame replayed already confirmed native lifecycle effects");
    auto conflict=batch;auto different=wait;different.source=l::Source::BossRespawn;conflict.rows[1].message=l::encode(different).dump();need(!client.prepare(origin,conflict,work,error),"conflicting lifecycle duplicate was silently deduplicated");
    need(host.pending()==batch,"packet send retired unacknowledged native lifecycle effects");need(host.acknowledge(origin,2,1,error),error);need(host.pending().after==3&&host.pending().rows.empty(),"confirmed whole Frame ACK did not retire covered native effects");
    need(client.prepare(origin,host.pending(),work,error)&&work.empty(),error);
    l::Client fresh;need(fresh.bind(origin,error),error);need(!fresh.prepare(origin,host.pending(),work,error),"Client lost its original invocation prefix but accepted a later ACK floor");
    auto wrong=origin;wrong.epoch++;need(!host.acknowledge(wrong,2,1,error),"native lifecycle ACK crossed actual World origin");
    auto malformed=batch;malformed.rows[0].message=" "+malformed.rows[0].message;need(!l::valid(malformed,error),"noncanonical nested lifecycle value accepted");malformed=batch;malformed.rows[0].message="[]";need(!l::valid(malformed,error),"arbitrary nested state was accepted as native invocation");malformed=batch;malformed.rows[0].message=std::string(8193,'x');need(!l::valid(malformed,error),"unbounded native lifecycle payload accepted");
}
}
int main()try{domain();journal();std::cout<<checks<<" actual Actor lifecycle grammar, between-Frame cycle, prefix, dedup and whole Frame ACK assertions passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
