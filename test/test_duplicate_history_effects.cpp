#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "duplicate_history_effects.h"
namespace effects=mt2mp::game::duplicate_history_effects;
namespace sh=mt2mp::game::duplicate_service_history;
namespace aq=mt2mp::action_session;
int main()try{
    std::string error,first,second;sh::Saved saved;saved.uid=10;saved.owner_uid=100;saved.type=2;
    saved.price={0,25};saved.counts[0].entries={1,2};saved.counts[0].words={1,2,3,4,5,6};
    sh::World services{{100,{saved}}};std::map<uint64_t,aq::ServiceMembershipState> states;
    auto& state=states[100];state.building_definition="SmallInn";state.spawn_type="Spawn";state.rows.resize(1);
    state.rows[0].type=2;state.rows[0].price={0,25};state.characters={{0,"Warrior",false}};
    state.config_present=true;state.configs={{"SmallInn_Spawn",{0,25}}};std::map<uint64_t,std::string> keys{{100,"action:inn"}};
    f::need(effects::services(services,states,keys,first,error),error);
    auto moved=saved;moved.uid=99;moved.owner_uid=200;moved.object=0x10000;moved.owner=0x20000;
    f::need(effects::services({{200,{moved}}},{{200,state}},{{200,"action:inn"}},second,error)&&first==second,
        "local Service pointers/full UIDs entered canonical complete witness");
    auto changed=services;changed.at(100)[0].counts[0].entries[1]++;
    f::need(effects::services(changed,states,keys,second,error)&&first!=second,"native full DailyCount entries were omitted");
    auto changed_states=states;changed_states.at(100).rows[0].character=0;
    f::need(effects::services(services,changed_states,keys,second,error)&&first!=second,"Service Character binding was omitted");
    changed_states=states;changed_states.at(100).configs[0].price[1]++;
    f::need(effects::services(services,changed_states,keys,second,error)&&first!=second,"effective Config Price owner was omitted");
    f::need(!effects::services(services,states,{},second,error),"missing complete canonical Building set admitted");
    using QuestWitness=mt2mp::game::monster_quest_retirement::Witness;QuestWitness quests;
    quests.manager=0x100;quests.completed=1;quests.quests={0x200};quests.links={{0x300,0x301,0x302,0x400,0x401,0x402}};
    auto& owner=quests.owners[0x400];owner.pointer=0x400;owner.uid=22;owner.key="action:npc";
    mt2mp::npc_quest_records::Record record;record.sequence_id=0;record.name="Visit the inn";record.description="A complete record";
    record.type=1;record.action=2;record.destination_key="action:inn";record.completed.total=1;record.rating.beauty=1.5f;
    owner.state.records={record};mt2mp::npc_quest_records::Local local;local.quest=0x200;local.owner=0x400;
    local.destination=0x300;local.npc_list_member=local.owner_weak_confirmed=local.destination_weak_confirmed=local.manager_member=true;
    owner.state.local={local};quests.rows={{owner.key,record}};
    f::need(effects::quests(quests,first,error),error);auto remapped=quests;auto copy=owner;remapped.owners.clear();
    copy.pointer=0x1400;copy.uid=222;copy.state.local[0].owner=0x1400;copy.state.local[0].quest=0x1200;copy.state.local[0].destination=0x1300;
    remapped.owners[0x1400]=copy;remapped.quests={0x1200};remapped.links={{0x1300,0x1301,0x1302,0x1400,0x1401,0x1402}};
    f::need(effects::quests(remapped,second,error)&&first==second,"local Quest pointers/full UIDs entered canonical complete witness");
    auto altered=quests;altered.owners.at(0x400).state.local[0].task=mt2mp::npc_quest_records::TaskStatus::Pending;
    f::need(effects::quests(altered,second,error)&&first!=second,"Quest native task phase was omitted");
    altered=quests;altered.rows[0].record.gold_reward++;
    f::need(!effects::quests(altered,second,error),"global Quest record disagreed with its current NPC owner");
    altered=quests;altered.completed=0;f::need(!effects::quests(altered,second,error),"Quest completed aggregate mismatch admitted");
    std::map<std::string,aq::DuplicateObject> objects;aq::DuplicateObject source,related,unrelated;
    source.key="action:source";source.family=aq::DuplicateFamily::Building;source.record_sha=std::string(64,'1');
    related=source;related.key="action:related";unrelated=source;unrelated.key="action:other";
    objects.emplace(source.key,source);objects.emplace(related.key,related);objects.emplace(unrelated.key,unrelated);
    aq::DuplicateState current;current.sources={source};std::string related_sha,untouched_sha;
    f::need(effects::prop_records(objects,current,{related.key},related_sha,untouched_sha,error),error);
    const auto prior_related=related_sha,prior_untouched=untouched_sha;objects.at(related.key).record_sha=std::string(64,'2');
    f::need(effects::prop_records(objects,current,{related.key},related_sha,untouched_sha,error)&&
        related_sha!=prior_related&&untouched_sha==prior_untouched,"bounded reciprocal owner was mixed into untouched props");
    objects.at(unrelated.key).record_sha=std::string(64,'3');
    f::need(effects::prop_records(objects,current,{related.key},related_sha,untouched_sha,error)&&untouched_sha!=prior_untouched,
        "unrelated changed prop escaped complete witness");
    objects.at(related.key).family=aq::DuplicateFamily::Scenery;
    f::need(!effects::prop_records(objects,current,{related.key},related_sha,untouched_sha,error),"related Service owner widened to Scenery");
    const std::string props(64,'4');
    std::vector<effects::OwnedChild> children{{"action:inn","action:inn/npc",std::string(64,'5'),true},
        {"action:art","","",false}};
    f::need(effects::owned_children(props,children,first,error),error);
    std::reverse(children.begin(),children.end());
    f::need(effects::owned_children(props,children,second,error)&&first==second,
        "owned child complete witness depends on census traversal order");
    children[1].record_sha=std::string(64,'6');
    f::need(effects::owned_children(props,children,second,error)&&first!=second,"owned child complete original Record was omitted");
    children.push_back(children[1]);
    f::need(!effects::owned_children(props,children,second,error),"duplicate canonical Building child receipt admitted");
    children.pop_back();children[0]={"action:other","action:inn/npc",std::string(64,'7'),true};
    f::need(!effects::owned_children(props,children,second,error),"same canonical child bound to two Building owners");
    children[0]={"action:art","","unexpected",false};
    f::need(!effects::owned_children(props,children,second,error),"absent child retained a success-shaped Record");
    children[0]={"action:art","","",false};children[1].record_sha="incomplete";
    f::need(!effects::owned_children(props,children,second,error),"present child accepted incomplete original Record hash");
    f::need(!effects::owned_children("",{},second,error),"owned child witness accepted absent related prop state");
    std::cout<<f::checks<<" complete canonical Service/Quest/related Building effect witness cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
