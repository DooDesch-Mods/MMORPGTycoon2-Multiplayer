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
# The Marketing fixture executes its application and ACK boundary. Exclude only
# that exact component hook from this independent existing-flow comparison.
marketing_call = 'if(!apply_marketing(frame,e))return false;'
assert body.count(marketing_call) == 1, 'one exact Marketing effect boundary required'
body = body.replace(marketing_call, '')
# VIP boundary tests execute these exact helpers and their failure latch.
# Remove only the new calls to compare all pre-existing gameplay and timings.
vip_prefix='bool vip_attempted=false,vip_completed=false;VipFailureLatch vip_latch{vip_attempted,vip_completed,locked_,receipt_,e};std::vector<vip::Transition> vip_work;if(!prepare_vips(frame,vip_work,e))return false;'
for inserted in [vip_prefix,'if(!apply_vips(frame,vip_work,vip_attempted,e))return false;','vip_completed=true;']:
    assert body.count(inserted)==1, 'one exact VIP effect boundary required'
    body=body.replace(inserted,'')
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
