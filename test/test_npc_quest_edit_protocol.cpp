#include "npc_quest_list_fixture.h"
#include <iostream>
int main()try{
    size_t checks=0;std::string e;std::vector<uint8_t> bytes;Message decoded;
    const auto check=[&](bool ok,const char* why){++checks;need(ok,why);};
    for(const auto edit:{QuestListEdit::Title,QuestListEdit::Kills,QuestListEdit::Rewards})for(bool defaults:{false,true}){
        auto m=result();m.request.action=Action::EditNpcQuest;auto& intent=m.request.npc_quest_list;
        intent.remove_slot=1;intent.edit=edit;
        if(edit==QuestListEdit::Title)intent.title="A precise new quest";
        if(edit==QuestListEdit::Kills)intent.kills=17;
        if(edit==QuestListEdit::Rewards){intent.defaults=defaults;if(!defaults){intent.xp=400;intent.gold=31;}}
        m.evidence.quest_list_after=m.evidence.quest_list_before;auto& after=m.evidence.quest_list_after;
        after.all_sha256=std::string(64,'b');++after.undo_count;
        auto& changed=after.records[1];
        if(edit==QuestListEdit::Title)changed.name=intent.title;
        if(edit==QuestListEdit::Kills)changed.monsters_to_kill=intent.kills;
        if(edit==QuestListEdit::Rewards){changed.xp_reward=defaults?-1:intent.xp;changed.gold_reward=defaults?-1:intent.gold;}
        check(encode(m,bytes,e),e.c_str());check(decode(bytes.data(),bytes.size(),decoded,e)&&decoded==m,"Quest edit schema35 full receipt roundtrip");
        const auto invalid=[&](auto change){auto bad=m;change(bad);check(!encode(bad,bytes,e),"Quest edit accepted a mismatched effect/domain");};
        invalid([](auto& x){x.request.action=Action::SetNpcQuestList;});
        invalid([](auto& x){x.request.npc_quest_list.edit=QuestListEdit::List;});
        invalid([](auto& x){x.request.npc_quest_list.remove_slot=-1;});
        invalid([](auto& x){x.request.npc_quest_list.remove_slot=5;});
        invalid([](auto& x){x.evidence.quest_list_after.records[0].name="Unrelated change";});
        invalid([](auto& x){++x.evidence.quest_list_after.records[1].action;});
        invalid([](auto& x){++x.evidence.quest_list_after.records[1].sequence_id;});
        invalid([](auto& x){++x.evidence.quest_list_after.records[1].accepted.total;});
        invalid([](auto& x){++x.evidence.quest_list_after.count;});
        invalid([](auto& x){++x.evidence.quest_list_after.completed;});
        invalid([](auto& x){++x.evidence.quest_list_after.global_random[0];});
        invalid([](auto& x){x.evidence.quest_list_after.references_sha256=std::string(64,'e');});
        invalid([](auto& x){x.evidence.quest_list_after.targets_sha256=std::string(64,'e');});
        invalid([](auto& x){x.evidence.quest_list_after.undo_count=0;});
        invalid([](auto& x){++x.evidence.after.cash;});
        invalid([](auto& x){x.request.npc_quest_list.title=std::string(1025,'x');});
        check(encode(m,bytes,e),e.c_str());auto raw=nlohmann::json::parse(bytes);
        raw["request"]["npc_quest_list"]["edit"]["defaults"]=0;auto text=raw.dump();
        check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,e),"Quest edit integer accepted as bool");
    }
    std::cout<<checks<<" Quest edit protocol and protected-effect checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
