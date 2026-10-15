"""Read both native worlds after one retained, already released movement input."""
import argparse
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
from local_acceptance import AcceptanceError,AcceptanceRunner,read_json,utc
from lobby_ui.native_local_acceptance import ready

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--test-root',type=Path,required=True)
parser.add_argument('--expected-run-id',required=True)
parser.add_argument('--attempt',type=Path,required=True)
args=parser.parse_args()
args.players,args.cleanup,args.step_timeout=2,False,45
runner=AcceptanceRunner(args)
runner.summary['scope']='read-only shared progress proof after retained exact-once input; no input replay'
try:
    runner.load_manifests()
    prior=read_json(args.attempt)
    if runner.summary['runtime_run_id']!=args.expected_run_id or prior['runtime_run_id']!=args.expected_run_id:
        raise AcceptanceError('Retained input and current owned run differ')
    checks={entry['check']:entry['result'] for entry in prior['checks']}
    receipt=checks['input-receipt']
    if not(receipt['down_events']==1 and receipt['up_events']==1 and receipt['axis_down'] and receipt['axis_released'] and
           not receipt['after']['key_down'] and receipt['after']['axis']==0 and receipt['after']['progress']>receipt['before']['progress']):
        raise AcceptanceError('Retained input/release and later native progress are incomplete')
    runner.checked('retained-input-with-original-false-failure',receipt)
    after={}
    for peer in ('host','client-1'):
        value=runner.single('progress-'+peer,peer,'game_read_tutorial_progress')
        baseline=checks['before-'+peer]
        if any(value[field]!=baseline[field] for field in ('snapshot_uuid','world_epoch','world_generation')):
            raise AcceptanceError('Actual local world changed on '+peer)
        rows=value['tutorial']['active']
        if len(rows)!=1 or rows[0]['scenario']!='initial_tutorial' or rows[0]['next']!=7 or value['client_sequence_fenced'] is not (peer=='client-1'):
            raise AcceptanceError('WASD cursor or native Client fence differs on '+peer)
        meter=rows[0]['message_progress']
        if meter!={'shown':True,'value':receipt['after']['progress'],'minimum':0.0,'maximum':100.0}:
            raise AcceptanceError('Actual native Meter differs from retained input result on '+peer)
        if not ready(runner.single('playing-'+peer,peer,'game_read_native_lobby')):
            raise AcceptanceError('Expected both peers still Playing')
        after[peer]=value
        runner.checked('progress-'+peer,value)
        runner.checked('frame-'+peer,runner.single('frame-'+peer,peer,'game_capture_frame'))
    if after['host']['tutorial']!=after['client-1']['tutorial']:
        raise AcceptanceError('Actual shared tutorial/progress state differs')
    runner.summary.update(passed=True,state='native-shared-movement-progress-confirmed',after=after,
                          original_input_receipt=receipt,original_attempt=str(args.attempt.resolve()),
                          diagnostic_timing_failure_retained=True)
except Exception as error:
    runner.summary.update(passed=False,state='failed-read-only',error={'step':runner.summary['step'],'cause':str(error)})
runner.summary['finished_utc']=utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)
