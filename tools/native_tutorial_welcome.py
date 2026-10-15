"""Close one owned Host welcome dialog and observe the ordinary native transition."""
import argparse
from pathlib import Path
import sys
import time
import uuid
sys.path.insert(0,str(Path(__file__).resolve().parent))
from local_acceptance import AcceptanceError,AcceptanceRunner,ROOT,read_json,utc

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--test-root',type=Path,required=True)
parser.add_argument('--expected-run-id',required=True)
parser.add_argument('--observation',type=Path,required=True)
parser.add_argument('--shared',action='store_true',help='Require Client adoption of the ordinary Host transition')
parser.add_argument('--require-progress',action='store_true',help='Read back the real Host and Client WASD Meter display')
args=parser.parse_args()
if args.require_progress and not args.shared:
    parser.error('--require-progress requires --shared before any native operation')
args.players,args.cleanup,args.step_timeout=2,False,45
runner=AcceptanceRunner(args)
runner.summary['scope']=('one original Host welcome close and shared Client step/history adoption'
                         if args.shared else 'one original Host welcome close; ordinary step/history readback, no tutorial synchronization claim')
def tutorial(value):
    rows=[row for row in value.get('tutorials',[]) if row.get('scenario_id')=='initial_tutorial']
    if len(rows)!=1:
        raise AcceptanceError('Exactly one active initial tutorial required')
    return rows[0]
try:
    runner.load_manifests()
    if runner.summary['runtime_run_id']!=args.expected_run_id:
        raise AcceptanceError('Owned run differs; no welcome operation attempted')
    source=read_json(args.observation)
    if source.get('passed') is not True or source.get('runtime_run_id')!=args.expected_run_id:
        raise AcceptanceError('Prior tutorial observation differs')
    before={peer:runner.single('before-'+peer,peer,'game_read_tutorial') for peer in ('host','client-1')}
    for peer,value in before.items():
        row=tutorial(value)
        if (value!=source['tutorials'][peer] or row.get('current_step_index')!=4 or row.get('current_step_id')!='welcome'
            or row.get('history_count')!=0 or row.get('current_step_complete') is not False):
            raise AcceptanceError('Expected unchanged native welcome on '+peer)
    host=before['host']
    arguments={'token':str(uuid.uuid4()),'snapshot_uuid':host['snapshot_uuid'],'world_epoch':host['world_epoch'],
               'world_generation':host['world_generation'],'current_step_index':4}
    effect=runner.single('host-welcome-ok-once','host','game_tutorial_welcome_ok',arguments)
    if effect.get('attempted') is not True or effect.get('confirmed') is not True or effect.get('unknown') is not False:
        raise AcceptanceError('Host welcome native hide not confirmed; no replay')
    runner.checked('native-welcome-hide',effect)
    deadline=time.monotonic()+30
    count=0
    while True:
        count+=1
        after=runner.single('host-following-tick-'+str(count),'host','game_read_tutorial')
        row=tutorial(after)
        if row['current_step_index']!=4:
            break
        if time.monotonic()>deadline:
            raise AcceptanceError('Host welcome progression timed out required<=30s; native hide not replayed')
    if row['current_step_index']!=6 or row['current_step_id']!='wasd' or row['history_count']!=1:
        raise AcceptanceError('Expected native wasd step6 and one welcome history entry')
    runner.checked('host-native-step-and-history',after)
    client_deadline=time.monotonic()+30
    client_count=0
    while True:
        client_count+=1
        client=runner.single('client-following-tick-'+str(client_count),'client-1','game_read_tutorial')
        if not args.shared:
            if client!=before['client-1']:
                raise AcceptanceError('Client unexpectedly changed before synchronization integration')
            runner.checked('client-independent-baseline',client)
            break
        if tutorial(client)==row:
            runner.checked('client-shared-native-step-and-history',client)
            break
        if time.monotonic()>client_deadline:
            raise AcceptanceError('Client tutorial adoption timed out required<=30s; Host action not replayed')
    if args.shared:
        from lobby_ui.native_local_acceptance import ready
        for peer in ('host','client-1'):
            state=runner.single('shared-playing-'+peer,peer,'game_read_native_lobby')
            if not ready(state):
                raise AcceptanceError('Shared transition lost active session on '+peer)
            runner.checked('shared-playing-'+peer,state)
    if args.require_progress:
        progress={}
        for peer in ('host','client-1'):
            value=runner.single('native-progress-'+peer,peer,'game_read_tutorial_progress')
            if (value.get('schema')!=1 or value.get('mainthread_confirmed') is not True or value.get('lifetime_confirmed') is not True
                or value.get('client_sequence_fenced') is not (peer=='client-1')):
                raise AcceptanceError('Native progress lifetime/fence incomplete on '+peer)
            reference=after if peer=='host' else client
            if any(value.get(field)!=reference[field] for field in ('snapshot_uuid','world_epoch','world_generation')):
                raise AcceptanceError('Progress read local origin differs on '+peer)
            rows=[row for row in value['tutorial']['active'] if row['scenario']=='initial_tutorial']
            if len(rows)!=1 or rows[0]['next']!=7 or len(rows[0]['history'])!=1:
                raise AcceptanceError('Expected native WASD cursor/history for progress read on '+peer)
            measured=rows[0]['message_progress']
            if measured!={'shown':True,'value':0.0,'minimum':0.0,'maximum':100.0}:
                raise AcceptanceError('Expected visible zero WASD progress before movement on '+peer)
            runner.checked('native-progress-'+peer,value)
            progress[peer]=value
        if progress['host']['tutorial']!=progress['client-1']['tutorial']:
            raise AcceptanceError('Actual tutorial display/cursor/history differ')
        runner.summary['progress']=progress
    for peer in ('host','client-1'):
        runner.checked('frame-'+peer,runner.single('frame-'+peer,peer,'game_capture_frame'))
    runner.summary.update(passed=True,state='native-shared-welcome-transition-confirmed' if args.shared else 'native-host-welcome-transition-confirmed',host=after,client=client)
except Exception as error:
    runner.summary.update(passed=False,state='failed-no-replay',error={'step':runner.summary['step'],'cause':str(error)})
runner.summary['finished_utc']=utc();runner.persist();print(runner.summary_path)
raise SystemExit(0 if runner.summary['passed'] else 1)
