"""Prove one fixed Host movement input and the joined Client progress delta."""
import argparse
from pathlib import Path
import sys
import time
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parent))
from local_acceptance import AcceptanceError,AcceptanceRunner,ROOT,utc,extract_result
from lobby_ui.native_local_acceptance import ready

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--test-root',type=Path,required=True)
parser.add_argument('--expected-run-id',required=True)
args=parser.parse_args()
args.players,args.cleanup,args.step_timeout=2,False,45
runner=AcceptanceRunner(args)
runner.summary['scope']='one owned Host W down/up receipt and matching Client native progress; no replay'

def meter(value):
    rows=value['tutorial']['active']
    if len(rows)!=1 or rows[0]['scenario']!='initial_tutorial' or rows[0]['next']!=7:
        raise AcceptanceError('Expected current initial tutorial WASD cursor7')
    progress=rows[0]['message_progress']
    if progress['shown'] is not True or progress['minimum']!=0 or progress['maximum']!=100:
        raise AcceptanceError('Expected visible WASD Meter0..100')
    return progress['value']

try:
    runner.load_manifests()
    if runner.summary['runtime_run_id']!=args.expected_run_id:
        raise AcceptanceError('Owned run differs; no input attempted')
    before={}
    for peer in ('host','client-1'):
        if not ready(runner.single('playing-before-'+peer,peer,'game_read_native_lobby')):
            raise AcceptanceError('Expected Playing before movement on '+peer)
        before[peer]=runner.single('before-'+peer,peer,'game_read_tutorial_progress')
        meter(before[peer])
        runner.checked('before-'+peer,before[peer])
    if before['host']['tutorial']!=before['client-1']['tutorial']:
        raise AcceptanceError('Initial native tutorial state differs')
    batch=runner.job('focus-and-movement-once','host',[
        {'tool':'game_tutorial_movement_focus','arguments':{'native_origin_uuid':before['host']['snapshot_uuid']}},
        {'tool':'game_read_tutorial_movement_focus','arguments':{}},
        {'tool':'game_tutorial_movement','arguments':{'acceptance_token':str(uuid.uuid4()),'native_origin_uuid':before['host']['snapshot_uuid']}},
        {'tool':'game_read_tutorial_movement','arguments':{}}])
    runner.checked('focus-request',extract_result(batch,'game_tutorial_movement_focus'))
    focus=extract_result(batch,'game_read_tutorial_movement_focus')
    if focus['ready'] is not True:
        raise AcceptanceError('Owned Host focus not confirmed; native movement guard remains authoritative')
    runner.checked('focus-confirmed',focus)
    effect=extract_result(batch,'game_tutorial_movement')
    runner.checked('queued-only',effect)
    receipt=extract_result(batch,'game_read_tutorial_movement')
    deadline=time.monotonic()+15
    while receipt['phase'] not in ('confirmed','failed'):
        receipt=runner.single('input-readback','host','game_read_tutorial_movement')
        if time.monotonic()>deadline:
            raise AcceptanceError('Native input receipt timed out required<=15s; no replay')
    runner.checked('input-receipt',receipt)
    if receipt['phase']!='confirmed' or not receipt['axis_released'] or receipt['down_events']!=1 or receipt['up_events']!=1:
        raise AcceptanceError('Input/progress/release failed: '+receipt['error']+'; release='+receipt['release_error'])
    after={}
    deadline=time.monotonic()+15
    while True:
        for peer in ('host','client-1'):
            after[peer]=runner.single('after-'+peer,peer,'game_read_tutorial_progress')
        if after['host']['tutorial']==after['client-1']['tutorial']:
            break
        if time.monotonic()>deadline:
            raise AcceptanceError('Client progress convergence timed out required<=15s')
    if not meter(before['host'])<meter(after['host'])<100:
        raise AcceptanceError('Expected real partial Host movement progress increase')
    if after['client-1']['client_sequence_fenced'] is not True:
        raise AcceptanceError('Client native Sequence fence lost')
    for peer in ('host','client-1'):
        runner.checked('after-'+peer,after[peer])
        if not ready(runner.single('playing-after-'+peer,peer,'game_read_native_lobby')):
            raise AcceptanceError('Shared movement lost Playing on '+peer)
        runner.checked('frame-'+peer,runner.single('frame-'+peer,peer,'game_capture_frame'))
    runner.summary.update(passed=True,state='native-shared-movement-progress-confirmed',before=before,after=after,receipt=receipt)
except Exception as error:
    runner.summary.update(passed=False,state='failed-no-replay',error={'step':runner.summary['step'],'cause':str(error)})
runner.summary['finished_utc']=utc()
runner.persist()
print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)
