"""TIME-ACCEPTANCE-1: owned running games only; never launch, reload, or replay input."""
from __future__ import annotations
import argparse
import ast
import hashlib
import json
from pathlib import Path
import re
import sys
import time
import uuid

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[3]
sys.path.insert(0,str(ROOT/'tools'))
from local_acceptance import AcceptanceRunner, AcceptanceError, read_json, write_json, utc

EXE_SHA='bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6'
TOOLS=('read_time_acceptance','native_time_key','read_time_input')
KEYS=('SPACE','F1','F2','F3','F4')
SCALES={'F1':0,'F2':1,'F3':3,'F4':9}
ORIGINS={'SPACE':{0x4a2a77,0x4a2d93},'F1':{0x4a2abe},'F2':{0x4a2b05},'F3':{0x4a2b4c},'F4':{0x4a2b97}}
INTENTS={'SPACE':'TogglePauseNormal','F1':'Pause','F2':'Normal','F3':'Fast','F4':'Fastest'}

def need(condition,cause):
    if not condition:raise AcceptanceError(cause)

def integer(value,minimum=0):
    return type(value) is int and minimum<=value<=18446744073709551615

def sdk_guard(path=ROOT/'tools/mcp/runtime.py'):
    operations=set()
    for node in ast.walk(ast.parse(path.read_text(encoding='utf-8'))):
        value=None
        if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='OPS' for t in node.targets):value=node.value
        if isinstance(node,ast.Call) and isinstance(node.func,ast.Attribute) and isinstance(node.func.value,ast.Name) and node.func.value.id=='OPS' and node.func.attr=='update' and len(node.args)==1:value=node.args[0]
        if value is not None:operations.update(ast.literal_eval(value))
    missing=sorted(set(TOOLS)-operations)
    need(not missing,'required MCP endpoints absent: '+', '.join(missing)+'; no game call or input attempted')

def identity(state,peer,run_id,dll_sha):
    need(state.get('schema_version')==1,'time acceptance schema_version required=1')
    expected={'run_id':run_id,'peer_id':peer,'dll_sha256':dll_sha,'exe_sha256':EXE_SHA}
    need(state.get('identity')==expected,f'time runtime/hash identity mismatch peer={peer}')

def ready(state,peer,run_id,dll_sha,mask,epoch=None,keys=KEYS):
    identity(state,peer,run_id,dll_sha)
    guards=state.get('guards',{})
    for key in ('owned_world','main_thread','world_active','session_connected','panel_input_clear','overlay_hidden','no_pending_operation','hook_origins_guarded'):
        need(guards.get(key) is True,f'time input guard failed peer={peer} guard={key} actual={guards.get(key)!r} required=true')
    readiness=state.get('key_readiness',{})
    for key in keys:
        entry=readiness.get(key,{})
        need(entry.get('ready') is True and entry.get('blocker')=='',f'time key guard failed peer={peer} key={key} readiness={entry!r}')
    need(state.get('peer_mask')==mask,f'authenticated peer mask changed peer={peer}')
    clock=state.get('time',{})
    need(integer(clock.get('epoch'),1) and (epoch is None or clock['epoch']==epoch),'time epoch absent or changed')
    need(integer(clock.get('revision'),1),'time revision absent')
    need(clock.get('phase')=='Active' and clock.get('hook_mask')==3 and clock.get('write_locked') is False,'time runtime not active with both hooks or write locked')
    need(clock.get('selected_scale') in (0,1,3,9) and type(clock.get('selected_scale')) in (int,float),'selected scale outside native UI domain')
    need(clock.get('pause_scale')==1,'native modal pause blocks ordinary input acceptance')
    need(clock.get('native_clock_scale')==(clock['selected_scale'] if peer==1 else 0),'native clock authority guard failed')
    need(state.get('keybindings')=={'SPACE':21,'F1':7,'F2':8,'F3':9,'F4':10},'current native physical key binding differs from required input IDs')
    counters=state.get('counters',{})
    for key in ('game_updates','ui_updates','host_setter_confirmed','client_clock_writes','business_day_forwarded','subscriber_day_forwarded'):
        need(integer(counters.get(key)),f'missing counter {key}')
    need(counters['client_clock_writes']==0,'client native time setter executed')
    presentation=state.get('presentation',{})
    need(presentation.get('epoch')==clock['epoch'] and presentation.get('revision')==clock['revision'],'presentation epoch/revision differs')
    need(presentation.get('selected_scale')==clock['selected_scale'] and presentation.get('pause_scale')==clock['pause_scale'],'presentation logical clock differs')
    items=presentation.get('items',[])
    need(len(items)==4 and sorted(i.get('scale',-1) for i in items)==[0,1,3,9],'four typed time Items required')
    for item in items:need(type(item.get('native_state')) is int and type(item.get('active')) is bool and item['active']==(item['native_state']==0),'typed Item readback malformed')
    if peer!=1:need([i['scale'] for i in items if i['active']]==[clock['selected_scale']],'client actor presentation differs from Host scale')
    return clock

class Sequence:
    def __init__(self,call,record,peers,run_id,dll_sha,timeout=30,now=time.monotonic,pause=time.sleep,keys=KEYS):
        self.call,self.record,self.peers=call,record,sorted(peers)
        self.run_id,self.dll_sha,self.timeout,self.now,self.pause=run_id,dll_sha,timeout,now,pause
        self.mask=sum(1<<(p-1) for p in peers);self.epoch=None;self.sequences={};self.mutations=[]
        self.keys=tuple(keys);need(self.keys in (tuple(KEYS),('F1','F2','F3','F4')),'keys require complete five-key scope or explicit four scale-key scope')

    def read(self,peer):
        state=self.call(peer,'game_read_time_acceptance',{})
        ready(state,peer,self.run_id,self.dll_sha,self.mask,self.epoch,self.keys)
        return state

    def poll(self,operation,condition):
        deadline=self.now()+self.timeout;last=None
        while True:
            done,last=condition()
            if done:return last
            need(self.now()<deadline,f'{operation} observation timeout; last={last}; no mutation replay')
            self.pause(min(0.1,max(0,deadline-self.now())))

    def input(self,peer,key,cleanup=False):
        before={p:self.read(p) for p in self.peers}
        host=before[1]['time'];target=SCALES.get(key,1 if host['selected_scale']*host['pause_scale']==0 else 0)
        need(all(v['time']['revision']==host['revision'] and v['time']['selected_scale']==host['selected_scale'] for v in before.values()),'peers not caught up before next input')
        token=str(uuid.uuid4());entry={'token':token,'peer':peer,'key':key,'cleanup':cleanup,'state':'attempted','before':before}
        self.mutations.append(entry);self.record('input_attempt',entry)
        processed=before[1].get('processed_sequences',{}).get(str(peer))
        need(integer(processed) and processed<18446744073709551615,'host processed sequence unavailable or exhausted')
        expected_sequence=processed+1
        response=self.call(peer,'game_native_time_key',{'key':key,'expected_epoch':self.epoch,'expected_revision':host['revision'],'expected_request_sequence':expected_sequence,'acceptance_token':token})
        need(response.get('acceptance_token')==token,'native input token mismatch')
        entry['queue_response']=response;self.record('input_queue',entry)
        need(response.get('state') in ('queued','pending','confirmed'),f'native input rejected or partial: {response}; no replay')
        def input_done():
            result=self.call(peer,'game_read_time_input',{'acceptance_token':token})
            need(result.get('acceptance_token')==token,'input observation token mismatch')
            need(result.get('state') in ('pending','confirmed'),f'native input failed/partial/unknown: {result}; no replay')
            return result['state']=='confirmed',result
        result=self.poll('native input '+token,input_done)
        entry['native_input']=result;self.record('input_observed',entry)
        need(result.get('down')=={'queued':True,'consumed':True} and result.get('up')=={'queued':True,'consumed':True},'both native SDL edges must be queued and consumed')
        need(result.get('held_after') is False,'native key still held after input')
        frames=result.get('frames',{})
        need(integer(frames.get('down_queued')) and integer(frames.get('down_consumed')) and integer(frames.get('up_queued')) and integer(frames.get('up_consumed')) and frames['down_queued']<=frames['up_queued'] and frames['down_queued']<frames['down_consumed']<=frames['up_consumed'] and frames['up_queued']<frames['up_consumed'],'native input frame ordering invalid')
        hook=result.get('hook',{})
        need(hook.get('caller_rva') in ORIGINS[key] and hook.get('origin')=='Keyboard' and hook.get('intent')==INTENTS[key] and hook.get('matched') is True and hook.get('captured_count')==1,'native UI boundary origin or exactly-one capture mismatch')
        seq=result.get('request_sequence');need(integer(seq,1) and seq==expected_sequence and seq>self.sequences.get(peer,0),'input request sequence missing, reused, or not the next Host-ordered request')
        receipt=result.get('host_receipt',{})
        need(receipt.get('epoch')==self.epoch and receipt.get('peer')==peer and receipt.get('sequence')==seq and receipt.get('observed_revision')==host['revision'],'host receipt identity mismatch')
        need(receipt.get('result') in ('Confirmed','NoChange') and receipt.get('code')=='None' and receipt.get('state_present') is True and receipt.get('uncertain') is False,'host receipt is not confirmed state')
        need(receipt.get('selected_scale')==target and receipt.get('pause_scale')==1,'host did not resolve input to expected scale')
        revision=receipt.get('revision');need(integer(revision,1) and revision==host['revision']+(target!=host['selected_scale']),'unexpected native clock revision')
        self.sequences[peer]=seq
        def all_done():
            after={p:self.read(p) for p in self.peers}
            okay=all(s['time']['revision']==revision and s['time']['selected_scale']==target for s in after.values())
            if okay:okay=all(after[1].get('presentation_acks',{}).get(str(p))==revision for p in self.peers if p!=1)
            return okay,after
        after=self.poll('peer presentation '+token,all_done)
        expected_writes=int(target!=host['selected_scale'])
        need(after[1]['counters']['host_setter_confirmed']-before[1]['counters']['host_setter_confirmed']==expected_writes,'host native setter execution count differs')
        need(all(after[p]['counters']['game_updates']>before[p]['counters']['game_updates'] for p in self.peers),'original game loop did not advance on every peer')
        entry.update(state='confirmed',after=after);self.record('input_confirmed',entry)
        return entry

    def run(self):
        initial={p:self.read(p) for p in self.peers};self.epoch=initial[1]['time']['epoch']
        need(all(v['time']['epoch']==self.epoch and v['time']['selected_scale']==0 for v in initial.values()),'same active epoch and initial paused selection required')
        self.record('initial',initial)
        for peer in self.peers:
            for key in self.keys:self.input(peer,key)
        self.input(1,'F1',cleanup=True)
        for peer in self.peers:self.record('frame',{'peer':peer,'result':self.call(peer,'game_capture_frame',{})})
        self.record('confirmed',{'scope':'native five-key route and shared typed actor presentation' if 'SPACE' in self.keys else 'native four scale keys and shared typed actor presentation','keys':list(self.keys),'Space_proven':'SPACE' in self.keys,'full_five_key_acceptance':tuple(self.keys)==tuple(KEYS),'native_calendar_authority_proven':False,'independent_client_economy_suppressed_proven':False,'visual_highlight_review':'pending','cleanup':'host native F1 confirmed'})

class Harness(AcceptanceRunner):
    def __init__(self,args):
        self.args,self.workspace,self.root=args,ROOT,args.test_root.resolve();self.id=str(uuid.uuid4())
        self.directory=HERE/'runs'/self.id;self.directory.mkdir(parents=True,exist_ok=False)
        self.summary_path=self.directory/'summary.json';self.manifests={};self.fingerprints={}
        self.summary={'schema_version':1,'acceptance_id':self.id,'state':'running','passed':False,'native_runtime_proof':False,'full_coop_proven':False,'keys':list(args.keys),'Space_proven':False,'full_five_key_acceptance':False,'jobs':[],'checks':[],'mutation_retries':0,'cleanup':{'state':'not_attempted'},'started_utc':utc()};self.persist()
        self.summary['scope']='native_five_time_keys' if 'SPACE' in args.keys else 'native_four_scale_keys';self.persist()

    def load(self):
        sdk_guard();ids={}
        for path in self.args.manifest:
            path=path.resolve();need(path.is_relative_to(self.root),'manifest outside owned test root');value=read_json(path);peer=value.get('peer_id');run=value.get('run_id')
            uuid.UUID(run);need(integer(peer,1) and peer<=4 and peer not in ids,'manifest duplicate or invalid peer')
            need(value.get('schema_version')==1 and value.get('owner')=={'kind':'MPTest','run_id':run},'manifest owner invalid')
            need(Path(value.get('runtime_root','')).resolve()==path.parent,'manifest runtime_root differs')
            need(type(value.get('pid')) is int and str(value.get('process_start_time','')).isdigit(),'manifest process identity absent')
            ids[peer]=value;self.manifests[peer]=path;self.fingerprints[peer]=hashlib.sha256(path.read_bytes()).hexdigest()
        need(1 in ids and 2<=len(ids)<=4,'host and one through three authenticated clients required')
        run=ids[1]['run_id'];need(all(v['run_id']==run for v in ids.values()),'manifest runs differ')
        self.summary['runtime_run_id']=run;self.summary['manifests']=ids;self.persist();return ids,run

    def call(self,peer,tool,args):
        need(hashlib.sha256(self.manifests[peer].read_bytes()).hexdigest()==self.fingerprints[peer],'manifest changed before job')
        result=self.single(tool[5:17],peer,tool,args)
        need(hashlib.sha256(self.manifests[peer].read_bytes()).hexdigest()==self.fingerprints[peer],'manifest changed during job')
        return result

    def record(self,name,value):
        self.checked(name,value)
        (self.directory/'progress.html').write_text('<!doctype html><meta charset="utf-8"><title>Time acceptance</title><h1>Time acceptance</h1><pre>'+__import__('html').escape(json.dumps({'step':name,'state':self.summary['state'],'native_runtime_proof':self.summary['native_runtime_proof']},indent=2))+'</pre>',encoding='utf-8')

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root',type=Path,required=True);parser.add_argument('--manifest',type=Path,action='append',required=True)
    parser.add_argument('--expected-dll-sha256',required=True);parser.add_argument('--step-timeout',type=float,default=30)
    parser.add_argument('--keys',nargs='+',choices=KEYS,default=list(KEYS))
    args=parser.parse_args(argv)
    if not re.fullmatch('[0-9a-f]{64}',args.expected_dll_sha256) or not 0.05<=args.step_timeout<=45:parser.error('exact lowercase DLL SHA256 and step-timeout 0.05..45 required')
    if tuple(args.keys) not in (tuple(KEYS),('F1','F2','F3','F4')):parser.error('keys require the default five keys or F1 F2 F3 F4')
    harness=Harness(args)
    try:
        peers,run=harness.load();sequence=Sequence(harness.call,harness.record,peers,run,args.expected_dll_sha256,args.step_timeout,keys=args.keys);sequence.run()
        harness.summary.update(state='confirmed',passed=True,native_runtime_proof=True,Space_proven='SPACE' in args.keys,full_five_key_acceptance=tuple(args.keys)==tuple(KEYS),cleanup={'state':'confirmed','operation':'host F1 native input'})
    except Exception as error:
        harness.summary.update(state='failed',error={'cause':f'{type(error).__name__}: {error}','behavior':'remaining mutations skipped; no replay, process cleanup, or automatic key repair'})
        print('error: '+harness.summary['error']['cause'],file=sys.stderr)
    finally:
        harness.summary['finished_utc']=utc();harness.persist()
    print(harness.summary_path);return 0 if harness.summary['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
