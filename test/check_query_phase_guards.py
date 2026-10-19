"""Compare the existing take flow outside timing wrappers and component hooks."""
from pathlib import Path
import argparse
import hashlib
import re

p = argparse.ArgumentParser()
p.add_argument('--source-root', type=Path, required=True)
a = p.parse_args()
text = (a.source_root / 'src/game/live_host_state/bridge.cpp').read_text(encoding='utf-8')
start = text.index('bool Bridge::take(')
end = text.index('\nbool Bridge::', start + 1)
body = text[start:end]
# Party/Instance effects use the same full-Frame failure latch and ACK. Check
# their ordering separately before comparing the pre-existing component flow.
party_prefix = 'bool party_attempted=false,party_completed=false;PartyFailureLatch party_latch{party_attempted,party_completed,locked_,receipt_,e};'
party_prepare = ('if(bool(frame.party)!=bool(party_port_))return fail(e,"Party complete component/provider presence differs before mutation");'
                 'if(party_port_){PartyReceipt prepared;const bool okay=party_port_->prepare_apply(context_,binding_,frame,prepared,e);'
                 'party_attempted=prepared.attempted||prepared.unknown;if(!okay||!prepared.confirmed)return fail(e,e.empty()?"Party/Instance structural receipt unconfirmed":e);}')
party_creation = ('if(party_port_){const auto state=economy_->addition_receipt();const auto& actual=state.at("last_creation");'
                  'if(actual.is_object()&&actual.value("frame_revision",uint64_t{})==frame.revision)'
                  'party_attempted=party_attempted||actual.value("attempted",false)||actual.value("unknown",false);}')
assert body.index(party_prefix) < body.index(party_prepare) < body.index('Pin before;'), 'Party structural boundary must precede native pre-state reads'
assert body.index('prepared=economy_->before_day') < body.index(party_creation), 'Party birth receipt must follow actual Economy application'
assert body.index('if(!acknowledge_frame(frame,e))') < body.index('party_completed=true;'), 'Party failure latch cannot close before full ACK'
for inserted in [party_prefix, party_prepare, party_creation, 'party_completed=true;']:
    assert body.count(inserted) == 1, 'one exact Party effect boundary required'
    body = body.replace(inserted, '')
ack_start = text.index('bool Bridge::acknowledge_frame(')
ack_end = text.index('\nbool Bridge::', ack_start + 1)
ack = text[ack_start:ack_end]
assert ack.index('transport_.send(') < ack.index('party_port_->committed(frame,e)'), 'Party action witnesses must survive until confirmed ACK send'
# Release135 adds this exact full-value boundary after Management and before
# presentation components and the full ACK. Compare the remaining original flow.
release_call = 'if(!apply_release(frame,e))return false;'
assert body.count(release_call) == 1, 'one exact Release effect boundary required'
assert body.index('if(!apply_management(frame,e))') < body.index(release_call), 'Release application must follow Management'
assert body.index(release_call) < body.index('if(!apply_vips(frame,vip_work,vip_attempted,e))'), 'Release application must precede VIP application'
assert body.index(release_call) < body.index('if(!apply_monsters(frame,e))'), 'Release application must precede Monster application'
assert body.index(release_call) < body.index('if(!acknowledge_frame(frame,e))'), 'Release application must precede full ACK'
body = body.replace(release_call, '')
# The Marketing fixture executes its application and ACK boundary. Exclude only
# that exact component hook from this independent existing-flow comparison.
marketing_call = 'if(!apply_marketing(frame,e))return false;'
assert body.count(marketing_call) == 1, 'one exact Marketing effect boundary required'
body = body.replace(marketing_call, '')
management_call = 'if(!apply_management(frame,e))return false;'
assert body.count(management_call) == 1, 'one exact Management effect boundary required'
assert body.index(management_call) < body.index('if(!acknowledge_frame(frame,e))'), 'Management application must precede full ACK'
body = body.replace(management_call, '')
# VIP boundary tests execute these exact helpers and their failure latch.
# Remove only the new calls to compare all pre-existing gameplay and timings.
vip_prefix='bool vip_attempted=false,vip_completed=false;VipFailureLatch vip_latch{vip_attempted,vip_completed,locked_,receipt_,e};std::vector<vip::Transition> vip_work;if(!prepare_vips(frame,vip_work,e))return false;'
for inserted in [vip_prefix,'if(!apply_vips(frame,vip_work,vip_attempted,e))return false;','vip_completed=true;']:
    assert body.count(inserted)==1, 'one exact VIP effect boundary required'
    body=body.replace(inserted,'')
# The Monster component has its own application and failure-boundary fixture.
# Keep this comparison on the existing flow while requiring its exact placement.
monster_preflight = ('if(bool(frame.monsters)!=bool(monster_port_))return fail(e,"Monster component/provider presence differs before mutation");\n'
                     'if(monster_port_&&!monster_port_->preflight(context_,binding_,frame,e))return false;\n')
monster_apply = 'if(!apply_monsters(frame,e))return false;'
assert body.count(monster_preflight) == 1, 'one exact Monster preflight boundary required'
assert body.count(monster_apply) == 1, 'one exact Monster application boundary required'
assert body.index(monster_preflight) < body.index('if(applying_economy_)'), 'Monster preflight must precede mutations'
assert body.index(monster_apply) < body.index('if(!acknowledge_frame(frame,e))'), 'Monster application must precede full ACK'
body = body.replace(monster_preflight, '').replace(monster_apply, '')
pattern = re.compile(r'\(\[&\]\{apply_timing::Scope measured_group\(apply_timing::Part::(\w+)\);return (.*?);\}\(\)\)', re.S)
expected = {'BugPreflight', 'BugAdopt', 'BugFinish', 'DeveloperPreflight', 'DeveloperAdopt',
            'NetworkPreflight', 'NetworkApply', 'TutorialPreflight', 'TutorialApply'}
# The full-Apply ACK body is checked by the actual frame-fragment fixture.
# Restore only its former inline block for the unchanged gameplay hash.
ack_call = 'if(!acknowledge_frame(frame,e))return false;'
assert body.count(ack_call) == 1, "one full-Apply ACK call required"
body = body.replace(ack_call, 'if(economy_&&queued_economy_->additions.available){\n    const auto rows=mt2mp::subscriber_lifecycle::additions::Cache::acknowledgement(queued_economy_->additions);\n    if(!rows.empty()){\n        const auto text=nlohmann::json{{"kind","host_state_subscriber_ack"},{"schema",1},{"session",context_.session},{"epoch",common_epoch_},{"snapshot_uuid",context_.snapshot_uuid},{"source_generation",frame.source_generation},{"frame_revision",frame.revision},{"rows",rows}}.dump();\n        std::vector<uint8_t> bytes(text.begin(),text.end());\n        if(bytes.size()>max_frame_payload||!transport_.send(context_.host_peer,bytes,e)){locked_=true;receipt_.unknown=true;receipt_.confirmed=false;return fail(e,"Subscriber confirmed Frame ACK send failed original="+e+"; no native replay");}\n    }\n    economy_->subscriber_frame_committed(frame.revision);\n}')
matches = pattern.findall(body)
assert len(matches) == 9 and {phase for phase, _ in matches} == expected, 'nine distinct timing groups required'
original = pattern.sub(lambda match: match[2], body)
assert hashlib.sha256(original.encode()).hexdigest() == '6fd76cdad09b467e3a398ca5249f8a1261745dd27b5dddbe1f05664444f9d2c8', 'original take behavior changed beyond timers'
print('PASS nine timing wrappers; complete original take body unchanged')
