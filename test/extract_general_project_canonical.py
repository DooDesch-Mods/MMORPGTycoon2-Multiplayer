from pathlib import Path
import argparse,hashlib,json
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
path=a.source_root/'src/game/typed_action/ordered_runtime.h';s=path.read_text()
start='snapshot_command_order_=command.order;last_evidence_sha256_.clear();';end='                 return outcome;'
assert s.count(start)==s.count(end)==1
producer=s[s.index(start):s.index(end)+len(end)]
assert 'executor_->execute(command,last_execution_,detail)' in producer and 'queue.bind_evidence(command,digest,detail)' in producer
start='    nlohmann::json execution_status()const{';end='        if(const auto* time=std::get_if<TimeRequest>'
assert s.count(start)==s.count(end)==1
status=s[s.index(start):s.index(end)]+'        return {{"available",false}};\n    }\n'
assert 'requires_evidence(last_execution_.command.input)' in status and 'last_evidence_sha256_' in status
start='        if(last_execution_tick_==frame->tick';end='last_execution_committed_tick_=frame->tick;'
assert s.count(start)==s.count(end)==1
commit=s[s.index(start):s.index(end)+len(end)]
outputs={'project_canonical_producer_actual.inc':producer,'project_canonical_status_actual.inc':status,'project_canonical_commit_actual.inc':commit}
for name,body in outputs.items():(a.output/name).write_text(body+'\n',newline='\n')
(a.output/'extraction.json').write_text(json.dumps({'source':str(path),'source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'bodies':{n:hashlib.sha256((a.output/n).read_bytes()).hexdigest() for n in outputs},'native_executed':False},indent=2)+'\n')
print('Extracted actual ordered canonical producer, GeneralProject-visible status branch and committed tick publication')
