#include "npc_quest_list_fixture.h"
#include "npc_quest_list_read.h"
#include <iostream>
int main()try{
    size_t checks=0;std::string e;std::vector<uint8_t> bytes;Message out;
    auto check=[&](bool ok,const char* label){++checks;need(ok,label);};
    for(bool remove:{false,true}){removing=remove;
        for(auto m:{request(),result()}){check(encode(m,bytes,e),e.c_str());check(decode(bytes.data(),bytes.size(),out,e)&&out==m,"full list schema35 roundtrip");}
        check(npc_quest_list_transition(intent(),before(),after(before())),"native list delta");
        auto invalid=[&](auto change){auto m=result();change(m);check(!encode(m,bytes,e),"changed list evidence accepted");};
        invalid([](auto& m){++m.evidence.quest_list_after.count;});
        invalid([](auto& m){++m.evidence.quest_list_after.completed;});
        invalid([](auto& m){m.evidence.quest_list_after.records.front().name="Changed survivor";});
        invalid([](auto& m){++m.evidence.quest_list_after.records.front().sequence_id;});
        invalid([](auto& m){m.evidence.quest_list_after.targets_sha256=std::string(64,'f');});
        invalid([](auto& m){m.evidence.quest_list_after.undo_count=0;});
        invalid([](auto& m){++m.evidence.after.cash;});
        invalid([](auto& m){m.request.npc_quest_list.remove_slot=-2;});
        invalid([](auto& m){m.request.npc_quest.before.sequence_id=0;});
        invalid([](auto& m){m.evidence.quest_before.record.sequence_id=0;});
        auto capped=before();capped.undo_count=20;auto capintent=intent();capintent.before=capped.records;
        check(npc_quest_list_transition(capintent,capped,after(capped)),"capped native Undo");
        check(encode(request(),bytes,e),e.c_str());auto raw=nlohmann::json::parse(bytes);check(raw["schema"]==35,"schema35");
        for(const char* field:{"region","remove_slot"}){auto bad=raw;bad["request"]["npc_quest_list"][field]=true;auto text=bad.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"boolean integer accepted");}
        {auto bad=raw;bad["schema"]=34;auto text=bad.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"wrong schema accepted");}
        {auto saved=result();saved.request.canonical_record_key=std::string(64,'a')+":world:npc:1";check(encode(saved,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==saved,"saved NPC canonical owner key");}
        {auto saved=result();const auto key=std::string(64,'a')+":world:monster:7";
            saved.request.npc_quest_list.before[1].destination_key=key;saved.evidence.quest_list_before.records[1].destination_key=key;
            saved.evidence.quest_list_after.records[remove?0:1].destination_key=key;
            check(encode(saved,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==saved,"SavedIdentity full Monster destination key retained");}
    }
    removing=false;
    {auto b=before();b.records.clear();b.count=2;b.completed=0;NpcQuestListIntent i{67,-1,{}};auto a=b;auto r=record(0);r.accepted={};r.completed={};a.records={r};++a.count;++a.undo_count;a.all_sha256=std::string(64,'b');check(npc_quest_list_transition(i,b,a),"Add from empty native owner");}
    {auto b=before();b.records.resize(1);b.count=3;b.completed=1;NpcQuestListIntent i{67,0,b.records};auto a=b;a.records.clear();--a.count;--a.completed;++a.undo_count;a.all_sha256=std::string(64,'b');check(npc_quest_list_transition(i,b,a),"Remove final owner slot");}
    {auto b=before();b.records[0].completed.total=9;auto a=after(b);removing=true;a=b;a.records.erase(a.records.begin());for(size_t i=0;i<a.records.size();++i)a.records[i].sequence_id=int32_t(i);--a.count;--a.completed;++a.undo_count;a.all_sha256=std::string(64,'b');NpcQuestListIntent i{67,0,b.records};check(npc_quest_list_transition(i,b,a),"completedManager decreases one qualifying Quest rather than total9");}
    namespace read=mt2mp::game::npc_quest_list_read;
    nlohmann::json refs=nlohmann::json::array({{{"id",4},{"progress",nlohmann::json::array({{{"npc",{{"state",2},{"key",owner_key}}},{"quest",{{"state",2},{"key",owner_key+"/quest/0"}}},{"complete",true},{"completed",2}},{{"quest",{{"state",2},{"key",owner_key+"/quest/2"}}},{"complete",false}}})}}});
    auto after_refs=refs;after_refs[0]["progress"][0]["quest"]={{"state",0},{"key",""}};after_refs[0]["progress"][0]["complete"]=false;
    after_refs[0]["progress"][1]["quest"]["key"]=owner_key+"/quest/1";
    check(read::references(refs,after_refs,owner_key,0,e),"normal original removed link and survivor renumber");
    after_refs[0]["progress"][0]["quest"]["state"]=1;check(read::references(refs,after_refs,owner_key,0,e),"expired native weak link without freed dereference");
    {auto bad=after_refs;bad[0]["progress"][0]["quest"]={{"state",2},{"key",owner_key+"/quest/0"}};check(!read::references(refs,bad,owner_key,0,e),"removed live reference accepted");}
    {auto bad=after_refs;bad[0]["progress"][0]["complete"]=true;check(!read::references(refs,bad,owner_key,0,e),"missing original QuestRemoved complete-flag effect accepted");}
    {auto bad=after_refs;bad[0]["progress"][1]["quest"]["key"]=owner_key+"/quest/2";check(!read::references(refs,bad,owner_key,0,e),"stale surviving slot accepted");}
    {auto bad=after_refs;bad[0]["progress"][0]["completed"]=3;check(!read::references(refs,bad,owner_key,0,e),"unrelated active counter changed");}
    check(read::references(refs,refs,owner_key,-1,e),"Add preserves all actual references");
    check(!read::references(refs,after_refs,owner_key,-1,e),"Add reference change accepted");
    namespace qr=mt2mp::game::monster_quest_retirement;
    auto witness=[&](){qr::Witness w;w.manager=0x800;w.completed=3;qr::Owner owner;owner.pointer=0x100;owner.uid=0xffff000000000001;owner.key=owner_key;
        for(size_t i=0;i<3;++i){const uintptr_t pointer=0x1000+i*0x1000,destination=0x5000+i*0x1000;
            auto r=record(int32_t(i));owner.state.records.push_back(r);
            owner.state.local.push_back({pointer,owner.pointer,destination,true,true,true,true,mt2mp::npc_quest_records::TaskStatus::Pending});
            owner.destination_uids.push_back(0xaaaa000000000001+i);w.quests.push_back(pointer);
            w.rows.push_back({owner.key,r});w.links.push_back({destination,destination+0x80,destination+0x20,owner.pointer,owner.pointer+0x80,owner.pointer+0x20});}
        w.owners.emplace(owner.pointer,owner);return w;};
    auto old=witness();auto added=old;added.quests.push_back(0x4000);auto r=record(3);r.accepted={};r.completed={};
    added.rows.push_back({owner_key,r});added.links.push_back({0x9000,0x9080,0x9020,0x100,0x180,0x120});
    auto& added_owner=added.owners.at(0x100);added_owner.state.records.push_back(r);added_owner.destination_uids.push_back(0xaaaa000000000009);
    added_owner.state.local.push_back({0x4000,0x100,0x9000,true,true,true,true,mt2mp::npc_quest_records::TaskStatus::Pending});
    check(read::added(old,added,0x100,e),"actual local new pointer, task and global append witness");
    {auto bad=added;bad.owners.at(0x100).uid^=uint64_t(1)<<48;check(!read::added(old,bad,0x100,e),"truncated owner UID accepted");}
    {auto bad=added;bad.rows[0].record.name="Changed";check(!read::added(old,bad,0x100,e),"unrelated global row changed");}
    {auto bad=added;bad.owners.at(0x100).state.local.back().task=mt2mp::npc_quest_records::TaskStatus::Absent;check(!read::added(old,bad,0x100,e),"missing native new rating task accepted");}
    {auto bad=added;std::swap(bad.quests[0],bad.quests[1]);check(!read::added(old,bad,0x100,e),"changed original global order accepted");}
    {auto bad=added;bad.quests.back()=old.quests[0];check(!read::added(old,bad,0x100,e),"reused native pointer accepted");}
    auto removed=old;old.removed={old.quests[0]};removed.quests={old.quests[2],old.quests[1]};removed.links={old.links[2],old.links[1]};--removed.completed;
    auto& remaining=removed.owners.at(0x100);remaining.state.records.erase(remaining.state.records.begin());remaining.state.local.erase(remaining.state.local.begin());remaining.destination_uids.erase(remaining.destination_uids.begin());
    for(size_t i=0;i<remaining.state.records.size();++i)remaining.state.records[i].sequence_id=int32_t(i);
    removed.rows={{owner_key,remaining.state.records[1]},{owner_key,remaining.state.records[0]}};
    check(qr::survivors(old,removed,e),"native global swap-last and owner order-preserving renumber");
    {auto bad=removed;bad.quests={old.quests[1],old.quests[2]};check(!qr::survivors(old,bad,e),"owner order substituted for global swap-last");}
    {auto bad=removed;bad.completed=old.completed;check(!qr::survivors(old,bad,e),"missing completedManager decrement accepted");}
    {auto bad=removed;bad.owners.at(0x100).state.records[0].sequence_id=1;check(!qr::survivors(old,bad,e),"survivor old slot accepted");}
    {auto bad=removed;bad.owners.at(0x100).state.local[0].quest=old.quests[0];check(!qr::survivors(old,bad,e),"removed pointer substituted for survivor");}
    std::cout<<"PASS "<<checks<<" Quest list protocol, transition and active-reference comparisons\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
