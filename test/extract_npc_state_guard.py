from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=(a.source_root/'src/game/live_host_state/day_rollover/economy/native_quest_state.inc').read_text();start='bool NativePort::store_npc_state(';assert s.count(start)==1
a.output.mkdir(parents=True,exist_ok=True);(a.output/'npc_state_guard.inc').write_text(s[s.index(start):])

s=(a.source_root/'src/game/live_host_state/day_rollover/economy/coupling.cpp').read_text()
start='bool Coupling::capture(';end='bool Coupling::read_current('
assert s.count(start)==1 and s.count(end)==1
(a.output/'subscriber_save_capture.inc').write_text(s[s.index(start):s.index(end)],encoding='utf-8')

def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
native=(a.source_root/'src/game/live_host_state/day_rollover/economy/native_monetary.cpp').read_text()
(a.output/'npc_current_receipt.inc').write_text('\n'.join(body(native,anchor) for anchor in ['bool NativePort::read(Owners&','bool NativePort::read_with_npc_owners(','bool NativePort::current(', 'bool NativePort::read_motion(', 'bool NativePort::current_npc_owners('])+'\n',encoding='utf-8')
# The original complete once reader is unchanged apart from output publication
# after its existing final validations. Test doubles only replace this leaf.
once=body(native,'bool NativePort::once(')
assert once.endswith('if(!canonical_subscribers(out,e)||!valid(out.state,e)||!finish_motion_pass(p,motion_pass,e))return false;if(npc_owners)*npc_owners=std::move(quest_owners);return true;}')
import hashlib
normalized=once.replace('std::string& e,std::vector<QuestOwner>* npc_owners)','std::string& e)').replace('if(!canonical_subscribers(out,e)||!valid(out.state,e)||!finish_motion_pass(p,motion_pass,e))return false;if(npc_owners)*npc_owners=std::move(quest_owners);return true;}','return canonical_subscribers(out,e)&&valid(out.state,e)&&finish_motion_pass(p,motion_pass,e);}')
assert hashlib.sha256(normalized.encode()).hexdigest()=="ab3e09af4b5f249a5263f0ea2b9445d5efe8eab36f00b14ebc18f31e30d9ab1b", 'Original complete once read body drifted'

motion=body(native,"bool NativePort::motion_once(")
assert motion.count("apply_timing::PageReadScope pages(!apply_timing::page_read_scope);")==1
motion=motion.replace("apply_timing::PageReadScope pages(!apply_timing::page_read_scope);","apply_timing::PageReadScope pages;",1)
assert hashlib.sha256(motion.encode()).hexdigest()=='1b43f613921d2afb7b2203f96032eddc8b5021baf96145633a45b186900bc2e1', "Existing complete motion ownership reader drifted"

quest=(a.source_root/'src/game/live_host_state/day_rollover/economy/native_quest_state.inc').read_text()
(a.output/'npc_daily_guard_actual.inc').write_text(body(quest,'struct QuestDailyWork {')+';\n'+body(quest,'bool quest_daily_body(')+'\n'+body(quest,'bool quest_daily_guard(')+'\n'+body(quest,'bool store_npc_counters(')+'\n')
scope=(a.source_root/'src/game/subscriber_lifecycle/save_scope.cpp').read_text()
(a.output/'npc_save_scope_actual.inc').write_text(body(scope,'bool callback(')+'\n'+body(scope,'bool begin_save_scope(')+'\n'+body(scope,'bool finish_save_scope(')+'\n'+body(scope,'bool with_save_scope(')+'\n')
baseline=body(quest,'bool store_npc_counters(')
scope_line='            apply_timing::PageReadScope pages;\n'
reserve_boundary='''        uint32_t exception=0;
        {
            apply_timing::PageReadScope original(apply_timing::PageBoundary::OriginalCall);
            exception=quest_daily_reserve(p.image+0xe8080,array,capacity);
        }'''
assert baseline.count(scope_line)==1 and baseline.count(reserve_boundary)==1, 'DailyCount metadata boundaries drifted'
baseline=baseline.replace(scope_line,'',1).replace(reserve_boundary,
    '        const auto exception=quest_daily_reserve(p.image+0xe8080,array,capacity);',1)
assert hashlib.sha256(baseline.encode()).hexdigest()=='5f422d3dce27936479abba0cd0b8f50aee177ccc6e8a0fdc9e1c01bed1357238', 'Pre-metadata complete counter body drifted'
(a.output/'npc_daily_metadata_baseline.inc').write_text(baseline.replace('bool store_npc_counters(',
    'bool store_npc_counters_metadata_baseline(',1)+'\n')
original=baseline
original=original.replace('    bool owner_proven=false;\n','').replace('        if(owner_proven)return true;\n','')
original=original.replace('        if(found==current.end()||found->npc!=owner.npc||found->quests!=owner.quests)\n            return fail(error,"DailyCount current canonical NPC/Quest owners changed");\n        owner_proven=true;return true;','        return (found!=current.end()&&found->npc==owner.npc&&found->quests==owner.quests)||\n            fail(error,"DailyCount current canonical NPC/Quest owners changed");')
original=original.replace('        // SaveScope enter precedes this body. Only fresh post-enter owners qualify.\n','').replace('        owner_proven=false;\n','')
original=original.replace('    const auto exit_guard=[&](std::string& error){\n        const bool okay=guard(error);owner_proven=false;return okay;\n    };\n    QuestDailyWork work{body,exit_guard};','    QuestDailyWork work{body,guard};')
assert hashlib.sha256(original.encode()).hexdigest()=="10fdb2309b5516c4af969caacff1afa39736568abf159c73e23e1f8bdc7867a1", 'Original complete counter body drifted'
(a.output/'npc_daily_guard_original.inc').write_text(original.replace('bool store_npc_counters(','bool store_npc_counters_original(',1)+'\n')

(a.output/"npc_values_actual.inc").write_text(body(quest,"bool quest_npc(")+"\n"+body(quest,"bool quest_npcs(")+"\n",encoding="utf-8")
