"""Verify the owned native Desktop and original shared New Game flow without OS input."""
import argparse
import hashlib
from pathlib import Path
import sys
import time
import json

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from local_acceptance import AcceptanceRunner,AcceptanceError,utc
from lobby_ui.framework_acceptance import FrameworkUI,visible
from lobby_ui.wizard_automation import complete_wizard
from runtime import candidate_binary_receipt

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--test-root',type=Path,required=True)
parser.add_argument('--expected-run-id',required=True)
parser.add_argument('--candidate-binary',type=Path,required=True)
parser.add_argument('--framework-sha256',required=True)
parser.add_argument('--desktop-proof',type=Path)
parser.add_argument('--handoff-proof',type=Path)
parser.add_argument('--wizard-observe-only',action='store_true')
parser.add_argument('--saved-world-setup',help='Load one exact owned saved fixture through the previously accepted shared-load route')
args=parser.parse_args()
args.players,args.cleanup,args.step_timeout,args.wait_seconds=2,False,45,120
runner=AcceptanceRunner(args)
runner.summary['scope']='Dedicated native Desktop single/drag/double, original wizard already-visible/hidden handoff, cancellation and native Create with automatic two-peer adoption. No gameplay action or reporting upload replay.'

def require(value,cause):
    if not value:raise AcceptanceError(cause)

def frame(label,peer):
    runner.checked(label,runner.single(label,peer,'game_capture_frame'))

def icon(ui,event):
    receipt=ui.request('desktop_activate',{'target':'app-icon:doodesch.multiplayer','event':event})
    require(receipt.get('confirmed') is True and receipt.get('native_returned') is True,
            'Actual native icon event unconfirmed; no replay: '+str(receipt))
    if event=='double':require(receipt.get('handler_reached') is True,'Native double-click did not reach its app')
    else:
        require(receipt.get('launches_unchanged') is True and receipt.get('cursor_fields_written') is False,
                'Single/drag changed launch count or wrote cursor fields')
        require(receipt['cursor_screen_before']==receipt['cursor_screen_after'],'Actual cursor moved')
    return receipt

try:
    runner.load_manifests()
    require(runner.summary['runtime_run_id']==args.expected_run_id,'Exact owned run differs')
    host,client=FrameworkUI(runner,'host'),FrameworkUI(runner,'client-1')
    for peer,ui in [('host',host),('client-1',client)]:
        runner.summary.setdefault('candidate_pins',{})[peer]=candidate_binary_receipt(ui.identity,args.candidate_binary)
        dll=Path(ui.identity['executable_path']).parent/'MT2Framework.dll'
        require(hashlib.sha256(dll.read_bytes()).hexdigest()==args.framework_sha256,'Exact Framework file differs')
        state=ui.wait(lambda v:v.get('framework_app',{}).get('desktop',{}).get('registered') is True,'desktop-registered')
        desktop=state['framework_app']['desktop']
        require(state['title_active'] and desktop['requested'] and desktop['visibility_known'] and desktop['visible'] is True and not desktop['error'],
                'Actual native Desktop entry is not visible and confirmed')
        require(not visible(state),'Registration opened the app without input')
    if args.wizard_observe_only:
        observed=host.request('desktop_activate',{'target':'wizard:doodesch.multiplayer:next','observe':True})
        require(observed.get('native_attempted') is False and isinstance(observed.get('guards'),dict),
                'Read-only wizard guard observation did not return its actual predicates')
        runner.summary.update(passed=True,state='native-wizard-guard-observed',finished_utc=utc(),
                              scope='Read actual original Next button guard values without activating it',
                              wizard_guard=observed)
        runner.persist();print(json.dumps(observed,ensure_ascii=False),flush=True);print(runner.summary_path);sys.exit(0)
    if args.desktop_proof:
        proof=json.loads(args.desktop_proof.read_text(encoding='utf-8'))
        require(proof['passed'] and [v['event'] for v in proof['events']]==['single','drag','double'],
                'Prior accepted Desktop scope is missing')
        runner.summary['inherited_desktop_proof']=str(args.desktop_proof)
    else:
        frame('dedicated-desktop-host','host')
        icon(host,'single')
        require(not visible(host.read()),'Single-click opened Multiplayer')
        # The original icon requires more than499ms between armed clicks.
        after=time.monotonic()+.55
        while time.monotonic()<after:client.read()
        icon(host,'drag')
        require(not visible(host.read()),'Dragging opened Multiplayer')
        frame('dragged-desktop-host','host')
    for ui in [host,client]:
        icon(ui,'double');ui.wait(visible,'dedicated-open-visible')
    client.click('join',lambda v:v['admitted'] and v['local_admitted'])
    hosted=host.click('host',lambda v:v['admitted'] and v['world_selection_open'])
    if args.saved_world_setup:
        require(hosted['saved_worlds'].count(args.saved_world_setup)==1,'Exact owned saved fixture is absent or duplicated')
        page,slot=divmod(hosted['saved_worlds'].index(args.saved_world_setup),hosted['ui_page_size'])
        for n in range(page):host.click('list_next',lambda v,target=n+1:v['discovery_page']==target)
        host.click('list'+str(slot),lambda v:v['selected_world']==args.saved_world_setup and
                   (v['prepare']['start_requested'] or v['prepare']['playing']))
        states={peer:ui.wait(lambda v:v['prepare']['playing'],'saved-fixture-playing') for peer,ui in [('host',host),('client-1',client)]}
        require(states['host']['prepare']['snapshot_id']==states['client-1']['prepare']['snapshot_id'],
                'Owned fixture peers did not acknowledge the same snapshot')
        runner.summary.update(passed=True,state='owned-shared-fixture-loaded',finished_utc=utc(),
                              scope='Measurement setup through previously accepted native shared-load route; no gameplay reacceptance',
                              dll_sha256=hashlib.sha256(args.candidate_binary.read_bytes()).hexdigest(),
                              loaded_world=args.saved_world_setup,
                              snapshot_id=states['host']['prepare']['snapshot_id'])
        runner.persist();print(runner.summary_path);sys.exit(0)
    first=host.click('new_world',lambda v:v.get('new_world',{}).get('active') is True)
    require(first['new_world']['handoff']['confirmed'] and first['new_world']['handoff']['status'] in ['opened','already_open'],
            'Original wizard handoff not confirmed')
    host.wait(lambda v:not visible(v),'wizard-handoff-hidden-app')
    client.wait(lambda v:v['loading_ui']['heading']=='Host is creating a world','remote-creating')
    frame('original-wizard-host','host');frame('creating-world-client','client-1')
    if args.handoff_proof:
        proof=json.loads(args.handoff_proof.read_text(encoding='utf-8'))
        require(proof['passed'] and proof['close']['close_confirmed'] and proof['opened']['status']=='opened',
                'Prior accepted original wizard handoff/cancel/reopen scope is missing')
        runner.summary['inherited_handoff_proof']=str(args.handoff_proof)
        runner.persist()
    else:
        closed=host.request('desktop_activate',{'target':'wizard:doodesch.multiplayer:close'})
        require(closed.get('close_confirmed') is True and closed.get('result')=='confirmed','Original wizard cancel was not confirmed; no replay')
        cancelled=host.wait(lambda v:visible(v) and v['world_selection_open'] and not v['new_world']['active'],'cancel-returned-picker')
        require(cancelled['new_world']['observation']['visible'] is False and cancelled['new_world']['observation']['attempted'] is False,
                'Cancellation observation reopened the wizard')
        client.wait(lambda v:v['loading_ui']['heading']=='Host is choosing a world','remote-choosing-after-cancel')
        opened=host.click('new_world',lambda v:v['new_world']['active'])
        require(opened['new_world']['handoff']['status']=='opened' and opened['new_world']['handoff']['attempted'] is True,
                'Hidden original wizard was not opened once')
        host.wait(lambda v:not visible(v),'second-wizard-handoff')
    complete_wizard(host,record=runner.checked,capture=lambda label:frame(label,'host'),
                    checkpoint=runner.checkpoint,wait_seconds=args.wait_seconds)
    runner.summary['native_create_returned']=True
    runner.persist()
    for peer,ui in [('host',host),('client-1',client)]:
        playing=ui.wait(lambda v:v['prepare']['playing'],'created-world-playing')
        require(not playing['new_world']['active'],'Wizard state survived successful world adoption')
        desktop=playing['framework_app']['desktop']
        require(desktop['visibility_known'] and desktop['visible'] is False,'Desktop icon remained visible in game')
        runner.summary.setdefault('created_worlds',{})[peer]={'selected_world':playing['selected_world'],'snapshot_id':playing['prepare']['snapshot_id']}
    require(runner.summary['created_worlds']['host']['snapshot_id']==runner.summary['created_worlds']['client-1']['snapshot_id'],
            'Peers did not acknowledge the same created world snapshot')
    require(runner.summary['created_worlds']['host']['selected_world'],'Host did not adopt the actual new world name')
    client.open()
    frame('created-world-mods-entry-client','client-1')
    runner.summary.update(passed=True,state='native-desktop-and-shared-creation-confirmed',finished_utc=utc(),
                          framework_sha256=args.framework_sha256,
                          limits=['No edited-name/seed/page native regression is claimed; already-open non-reset is separately covered by the Framework state test.',
                                  'No online, full gameplay, FPS or inactive-mod native acceptance is claimed.'])
    runner.persist()
except Exception as error:
    runner.summary.update(passed=False,state='failed-no-replay',error={'step':runner.summary['step'],'cause':str(error)},finished_utc=utc())
    runner.persist();print(runner.summary_path);sys.exit(1)
print(runner.summary_path)
