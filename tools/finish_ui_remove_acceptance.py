"""Observe an already admitted UI action without replaying its command."""
import argparse,hashlib,json,math,time,uuid
from pathlib import Path
from local_acceptance import AcceptanceRunner,AcceptanceError,ROOT,read_json,ledger_state,utc,log,hash_file
from runtime import Runtime,FRAMEWORK_PLUGIN_KIND,manifest_runtime_kind,candidate_binary_receipt
from native_remove_acceptance import retirement_confirmed
from ui_remove_acceptance import require_ui,actual_refund

def finite_timeout(text):
    value=float(text)
    if not math.isfinite(value) or not 0<value<=65:raise argparse.ArgumentTypeError('step timeout requires a finite value above0 and at most65 seconds')
    return value

def saved_result(summary,step,peer,tool):
    job=next(item for item in summary['jobs'] if item['step']==step)
    if job.get('state')!='confirmed' or job.get('peer')!=peer or job.get('tools')!=[tool]:
        raise AcceptanceError('saved UI job peer/tool/state differs: '+step)
    path=Path(job['evidence_path'])
    if hashlib.sha256(path.read_bytes()).hexdigest()!=job['evidence_sha256']:raise AcceptanceError('saved UI job changed: '+step)
    evidence=read_json(path)
    if evidence.get('passed') is not True:raise AcceptanceError('saved UI job was not confirmed: '+step+' cause='+str(evidence.get('error',{}).get('cause')))
    calls=evidence['calls']
    if len(calls)!=2 or calls[0].get('tool')!='bind_runtime' or calls[0].get('state')!='confirmed' or calls[0]['result'].get('isError') is not False:
        raise AcceptanceError('saved UI job actual binding acknowledgement differs: '+step)
    bound=calls[0]['result']['structuredContent'].get('bound')
    manifests=next(item['result'] for item in summary['checks'] if item['check']=='instance-manifest-identity')
    if bound!=manifests[peer]:raise AcceptanceError('saved UI job actual bound process differs: '+step)
    call=calls[-1]
    if call.get('tool')!=tool or call.get('state')!='confirmed' or call['result'].get('isError') is not False:
        raise AcceptanceError('saved UI job operation acknowledgement differs: '+step)
    response=call['result']['structuredContent']
    if response.get('ok') is not True or response.get('run_id')!=summary['runtime_run_id']:raise AcceptanceError('saved UI response run differs: '+step)
    return response['result']

def one_check(summary,name):
    values=[item['result'] for item in summary['checks'] if item.get('check')==name]
    if len(values)!=1:raise AcceptanceError('saved check missing or duplicated: '+name)
    return values[0]

def exact(left,right):
    return json.dumps(left,sort_keys=True,separators=(',',':'))==json.dumps(right,sort_keys=True,separators=(',',':'))

def validate_bind(evidence,manifest):
    calls=evidence.get('calls',[])
    if len(calls)!=2:raise AcceptanceError('recovery evidence requires exactly bind and one observation')
    call=calls[0];result=call.get('result',{});bound=result.get('structuredContent',{})
    if (call.get('tool')!='bind_runtime' or call.get('state')!='confirmed' or result.get('isError') is not False
            or bound.get('process_identity_verified') is not True or not exact(bound.get('bound'),manifest)
            or evidence.get('bind_skipped_for_recovery_only') is not False):
        raise AcceptanceError('recovery exact bound process differs')
    expected=str(Path(manifest['runtime_root'])/'instance.json')
    if Path(evidence.get('manifest_path','')).resolve()!=Path(expected).resolve() or Path(call.get('arguments',{}).get('manifest_path','')).resolve()!=Path(expected).resolve():
        raise AcceptanceError('recovery manifest path differs')
    return calls[-1]

def validated_spec(evidence):
    path=Path(evidence['spec_path'])
    if hashlib.sha256(path.read_bytes()).hexdigest()!=evidence.get('spec_sha256'):
        raise AcceptanceError('recovery job spec hash differs')
    spec=read_json(path)
    if not exact(spec,evidence.get('spec')):raise AcceptanceError('recovery job spec content differs')
    if evidence.get('retries')!=0:raise AcceptanceError('recovery job contains retries')
    return spec

def recovered_admission(summary,recovered_path,original_path):
    manifests=one_check(summary,'instance-manifest-identity');manifest=manifests['client-1']
    jobs=[item for item in summary['jobs'] if item.get('step')=='client-actual-BuildingWindow-command-once']
    if len(jobs)!=1:raise AcceptanceError('original Remove command job missing or duplicated')
    job=jobs[0]
    if job.get('state')!='failed' or job.get('peer')!='client-1' or job.get('tools')!=['game_probe_remove_ui']:
        raise AcceptanceError('original failed Remove job peer/tool/state differs')
    path=Path(job['evidence_path'])
    if hashlib.sha256(path.read_bytes()).hexdigest()!=job['evidence_sha256']:raise AcceptanceError('original failed Remove evidence changed')
    failed=read_json(path);call=validate_bind(failed,manifest);spec=validated_spec(failed)
    if (failed.get('passed') is not False or call.get('tool')!='game_probe_remove_ui' or call.get('state')!='failed'
            or call.get('result',{}).get('isError') is not True
            or not any(item.get('type')=='text' and item.get('text','').startswith('oversize_json: ') for item in call['result'].get('content',[]))):
        raise AcceptanceError('original failure is not retained Remove response-size rejection')
    if Path(job['spec_path']).resolve()!=Path(failed['spec_path']).resolve():raise AcceptanceError('original summary/spec path differs')
    arguments=call.get('arguments')
    if not exact(spec.get('calls'),[dict(tool='game_probe_remove_ui',arguments=arguments)]):raise AcceptanceError('original Remove spec/call differs')
    target=one_check(summary,'UI-actual-targets')['client-1']
    expected=dict(diagnostic_uuid=summary['diagnostic_uuid'],snapshot_uuid=target['snapshot_id'],record_key=target['record_key'])
    if not exact(arguments,expected):raise AcceptanceError('original Remove diagnostic/target arguments differ')
    retained=read_json(original_path);request=retained.get('request',{});lock=retained.get('lock',{})
    if retained.get('schema_version')!=1 or request.get('schema_version')!=1 or lock.get('schema_version')!=1:
        raise AcceptanceError('retained Remove request schema differs')
    identifier=request.get('id')
    if not isinstance(identifier,str) or str(uuid.UUID(identifier))!=identifier:raise AcceptanceError('retained Remove request UUID invalid')
    if (request.get('op')!='probe_remove_ui' or request.get('run_id')!=summary['runtime_run_id']
            or not exact(request.get('args'),arguments)):
        raise AcceptanceError('retained Remove request op/run/arguments differ')
    expected_lock=dict(schema_version=1,id=identifier,run_id=manifest['run_id'],pid=manifest['pid'],
        process_start_time=manifest['process_start_time'],op='probe_remove_ui',runtime_root=manifest['runtime_root'],
        executable_path=manifest['executable_path'],owner=manifest['owner'])
    if manifest_runtime_kind(manifest)==FRAMEWORK_PLUGIN_KIND:
        expected_lock.update(runtime_kind=FRAMEWORK_PLUGIN_KIND,framework_profile=manifest['framework_profile'])
    if not exact(lock,expected_lock):raise AcceptanceError('retained Remove lock request/process/owner differs')
    raw_path=Path(retained['response_path'])
    if hashlib.sha256(raw_path.read_bytes()).hexdigest()!=retained['response_sha256']:raise AcceptanceError('retained original response hash differs')
    raw=read_json(raw_path)
    recovery=read_json(recovered_path);observation=validate_bind(recovery,manifest);recovery_spec=validated_spec(recovery)
    entries=recovery_spec.get('calls',[])
    if (recovery.get('passed') is not True or len(entries)!=1 or entries[0].get('tool')!='observe_pending'
            or entries[0].get('arguments',{})!={} or set(entries[0])-{'tool','arguments'}
            or observation.get('tool')!='observe_pending' or observation.get('arguments')!={}
            or observation.get('state')!='confirmed' or observation.get('result',{}).get('isError') is not False):
        raise AcceptanceError('recovery is not one confirmed read-only pending observation')
    value=observation['result']['structuredContent']
    if value.get('pending') is not False or value.get('abandoned_after_process_exit') or not exact(value.get('response'),raw):
        raise AcceptanceError('recovery does not contain exact retained original response')
    if raw.get('schema_version')!=1 or raw.get('id')!=identifier or raw.get('run_id')!=summary['runtime_run_id'] or raw.get('ok') is not True:
        raise AcceptanceError('recovered response UUID/run/success differs')
    admitted=raw['result']
    if any(admitted.get(key)!=value for key,value in expected.items()):raise AcceptanceError('recovered diagnostic/target differs')
    return admitted

def baseline_counts(before):
    counters={}
    for peer in ('host','client-1'):
        state=before[peer]['typed_actions']
        if state.get('pending') is not False or state.get('failed')!=0 or state.get('locked') is not False or state.get('cause'):
            raise AcceptanceError('saved initial Controller is not healthy and idle: '+peer)
        values={key:state.get(key) for key in ('attempted','confirmed','host_revision')}
        if any(type(value) is not int or value<0 for value in values.values()):raise AcceptanceError('saved initial Controller counter type differs: '+peer)
        if counters and values!=counters:raise AcceptanceError('saved initial Controller counters differ between peers')
        counters=values
    return counters

def completion_counts(state,counters,peer):
    for field in ('attempted','confirmed','host_revision'):
        if type(state.get(field)) is not int or state[field]!=counters[field]+1:
            raise AcceptanceError('UI completion count differs: '+peer+':'+field)

def saved_refunds(summary,targets):
    recorded=one_check(summary,'UI-actual-native-refunds');refunds={}
    for peer in ('host','client-1'):
        bindings=saved_result(summary,peer+'-UI-binding',peer,'game_read_remove_bindings')
        matches=[row for row in bindings.get('bindings',[]) if row.get('record_key')==targets[peer]['record_key']]
        if len(matches)!=1 or not exact(matches[0],targets[peer]):raise AcceptanceError('saved native refund target differs: '+peer)
        refunds[peer]=actual_refund(bindings,targets[peer],peer)
        if type(recorded.get(peer)) is not int or recorded[peer]!=refunds[peer]:raise AcceptanceError('saved native refund check differs: '+peer)
    if refunds['host']!=refunds['client-1']:raise AcceptanceError('saved native refunds differ between peers')
    return refunds

def validate_candidates(runner,previous,candidate):
    original=one_check(previous,'instance-manifest-identity');expected=previous['parameters']['expected_dll_sha256']
    for peer in ('host','client-1'):
        path=runner.manifests[peer];identity=read_json(path)
        if not exact(identity,original[peer]):raise AcceptanceError('UI completion actual process identity changed: '+peer)
        if manifest_runtime_kind(identity)==FRAMEWORK_PLUGIN_KIND:
            bound=Runtime(runner.root).bind(str(path))
            if bound.get('process_identity_verified') is not True:raise AcceptanceError('UI completion Framework identity unverified: '+peer)
            receipt=candidate_binary_receipt(identity,candidate)
        elif candidate is not None:
            receipt=candidate_binary_receipt(identity,candidate)
            if hash_file(Path(identity['runtime_root'])/'game/MT2_Multiplayer.dll')!=expected:raise AcceptanceError('UI completion installed DLL bytes changed: '+peer)
        else:
            receipt=dict(sha256=identity['installed_binary_sha256']['MT2_Multiplayer.dll'])
        if receipt['sha256']!=expected:raise AcceptanceError('UI completion DLL changed: '+peer)
        runner.checked('selected-candidate-'+peer,receipt)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--admission-summary',type=Path,required=True)
    parser.add_argument('--step-timeout',type=finite_timeout,default=45)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--candidate-binary',type=Path)
    parser.add_argument('--recovered-admission',type=Path)
    parser.add_argument('--original-request',type=Path)
    args=parser.parse_args();args.players=2;args.cleanup=False
    if bool(args.recovered_admission)!=bool(args.original_request):parser.error('--recovered-admission and --original-request are required together')
    previous=read_json(args.admission_summary);runner=AcceptanceRunner(args)
    runner.summary['scope']='read-only completion of the previously admitted native BuildingInfoWindow action; no start/enable/selection/command/retry'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=previous['runtime_run_id']:raise AcceptanceError('UI completion exact owned run changed')
        validate_candidates(runner,previous,args.candidate_binary)
        targets=next(item['result'] for item in previous['checks'] if item['check']=='UI-actual-targets')
        before={peer:saved_result(previous,peer+'-UI-before',peer,'game_inspect_engine') for peer in ('host','client-1')}
        counters=baseline_counts(before);refunds=saved_refunds(previous,targets)
        if args.recovered_admission:
            admitted=recovered_admission(previous,args.recovered_admission,args.original_request)
            runner.checked('exact-recovered-admission-evidence',{str(path):hash_file(path) for path in (args.recovered_admission,args.original_request)})
        else:
            admitted=saved_result(previous,'client-actual-BuildingWindow-command-once','client-1','game_probe_remove_ui')
        if admitted['confirmed'] is not True or any(type(admitted.get(field)) is not int or admitted[field]!=value for field,value in
            (('ui_captured',1),('ui_replay_originals',0),('ui_disposition',3),('ui_origin',1))):
            raise AcceptanceError('saved admission does not identify the actual BuildingWindow intent')
        if admitted.get('diagnostic_uuid')!=previous.get('diagnostic_uuid') or not admitted.get('diagnostic_uuid'):
            raise AcceptanceError('saved UI admission diagnostic UUID differs')
        state=admitted['action_at_return'];identifier=admitted['action_uuid']
        if admitted['snapshot_uuid']!=targets['client-1']['snapshot_id'] or admitted['record_key']!=targets['client-1']['record_key']:
            raise AcceptanceError('saved UI admission target differs')
        receipt=admitted['receipt']
        for field in ('selection_attempted','selection_returned','window_confirmed','record_constructed','record_label_confirmed',
            'ui_command_attempted','ui_command_returned','ui_command_accepted','record_cleanup_attempted','record_cleanup_confirmed','intent_observed'):
            if receipt.get(field) is not True:raise AcceptanceError('saved UI native receipt incomplete: '+field)
        if any(type(receipt.get(field)) is not int or receipt[field]!=0 for field in ('native_exception','cleanup_exception')) or receipt.get('failed_stage') or receipt.get('cause'):
            raise AcceptanceError('saved UI native receipt retains failure: stage='+str(receipt.get('failed_stage'))+
                ' native_exception='+str(receipt.get('native_exception'))+' cleanup_exception='+str(receipt.get('cleanup_exception'))+' cause='+str(receipt.get('cause')))
        if state['action_uuid']!=identifier or state['pending'] is not True or any(type(state.get(field)) is not int or state[field]!=value for field,value in counters.items()):raise AcceptanceError('saved Client already executed before Host authority')
        if ledger_state(before['host'])!=ledger_state(before['client-1']):raise AcceptanceError('saved initial UI ledgers differ')
        runner.summary['action_uuid']=identifier;runner.summary['admission_summary']=str(args.admission_summary)
        runner.checked('saved-actual-UI-admission-before-Host',admitted)
        deadline=time.monotonic()+args.step_timeout;after={}
        for peer in ('host','client-1'):
            count=0
            while True:
                if time.monotonic()>=deadline:raise AcceptanceError('UI completion observation timed out: '+peer+'; no command replay')
                count+=1;engine=runner.inspect(peer+f'-retained-UI-completion-{count:02d}',peer);state=engine['typed_actions']
                if state['locked']:raise AcceptanceError('UI completion Unknown: '+state.get('cause',''))
                if state['action_uuid']==identifier and state.get('pending') is False and state.get('outcome') in (2,3):
                    raise AcceptanceError('UI action terminated: outcome='+str(state.get('outcome'))+' cause='+str(state.get('cause')))
                if state['action_uuid']==identifier and state['outcome']==1 and state['pending'] is False:
                    for field in ('outcome','attempted','confirmed','host_revision','failed','ack_mask','required_mask'):
                        if type(state.get(field)) is not int:raise AcceptanceError('UI completion scalar type differs: '+peer+':'+field)
                    completion_counts(state,counters,peer)
                    if state['failed']!=0 or state['cause']:raise AcceptanceError('UI completion retained failure: '+peer+' failed='+str(state['failed'])+' cause='+str(state['cause']))
                    if peer=='host' and (state['required_mask']!=3 or state['ack_mask']!=3):raise AcceptanceError('UI completion Host lacks both ACKs')
                    require_ui(engine['ui_remove'],1 if peer=='client-1' else 0,1,peer)
                    removed=engine['native_remove'];receipt=removed['receipts'][identifier]
                    if receipt['confirmed'] is not True or receipt['binding_revoked'] is not True:raise AcceptanceError('UI removal receipt absent: '+peer)
                    if receipt.get('token')!=identifier or receipt.get('snapshot_id')!=targets[peer]['snapshot_id'] or receipt.get('record_key')!=targets[peer]['record_key'] or receipt.get('error'):
                        raise AcceptanceError('UI removal actual token/target/cause differs: '+peer)
                    origin=before[peer]['native_open_origin']
                    if receipt.get('world_epoch')!=origin['world_epoch'] or receipt.get('world_generation')!=origin['generation']:
                        raise AcceptanceError('UI removal original world differs: '+peer)
                    native=receipt.get('result',{})
                    for field in ('attempted','returned','confirmed','target_retired','selection_confirmed','undo_confirmed'):
                        if native.get(field) is not True:raise AcceptanceError('UI native removal unconfirmed: '+peer+':'+field)
                    if type(native.get('seh_code')) is not int or native['seh_code']!=0 or native.get('unknown_locked') is not False or native.get('error') or native.get('retirement_cause'):
                        raise AcceptanceError('UI native removal Unknown or original failure: '+peer+' seh_code='+str(native.get('seh_code'))+
                            ' cause='+str(native.get('error'))+' retirement_cause='+str(native.get('retirement_cause')))
                    if type(receipt.get('sequence')) is not int or receipt['sequence']<=0 or type(native.get('sequence')) is not int or native['sequence']!=receipt['sequence']:
                        raise AcceptanceError('UI native removal sequence differs: '+peer)
                    retirement_confirmed(removed['retirements'][identifier],identifier,receipt['sequence'])
                    if any(item.get('uid')==targets[peer]['local_uid'] for item in engine['props']):raise AcceptanceError('UI target still present: '+peer)
                    after[peer]=engine;runner.checked('actual-UI-authoritative-completion',{'peer':peer,'state':state,'ui':engine['ui_remove']});break
        ledger=ledger_state(after['host'])
        if ledger!=ledger_state(after['client-1']) or ledger['cash']!=ledger_state(before['host'])['cash']+refunds['host']:
            raise AcceptanceError('UI final ledger or saved native refund differs')
        runner.checked('actual-UI-native-ledgers',ledger)
        for peer in ('host','client-1'):runner.capture(peer)
        runner.summary.update(state='confirmed-native-UI-remove-two',passed=True,regular_UI_capture_proven=True,
            client_request_replication_proven=True,holder_retirement_proven=True,UI_command_replayed=False)
    except Exception as cause:
        runner.summary.update(state='failed-retained-UI-remove-two',passed=False,error={'step':runner.summary['step'],
            'cause':str(cause),'behavior':'observation stopped; no UI command or native replay'})
        log('error','UI completion: '+str(cause))
    runner.summary['finished_utc']=utc();runner.persist()
    log('info' if runner.summary['passed'] else 'error','UI completion passed='+str(runner.summary['passed'])+' summary='+str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1
if __name__=='__main__':raise SystemExit(main())
