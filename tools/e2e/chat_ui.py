"""Exercise actual F9, Unicode text, Enter and Escape through the game MCP."""
import argparse
import sys
import time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
sys.path.insert(0,str(ROOT/'tools/mcp'))
sys.path.insert(0,str(ROOT))
from local_acceptance import AcceptanceRunner, AcceptanceError, utc, log
from tools.e2e.run import preflight

def observe(runner,peer,label,predicate,seconds):
    deadline=time.monotonic()+seconds
    index=0
    while time.monotonic()<deadline-3:
        index+=1
        state=runner.single(label+'-'+str(index),peer,'game_read_chat_ui',{})
        if predicate(state):
            runner.checked(label,state)
            return state
    raise AcceptanceError('actual chat UI state not confirmed: '+label+'; no input replay')

def recipient(runner,peer,text,origin,seconds):
    deadline=time.monotonic()+seconds
    index=0
    while time.monotonic()<deadline-3:
        index+=1
        states={name:runner.single(peer+'-delivery-'+str(index)+'-'+name,name,'game_read_chat',{}) for name in ('host','client-1')}
        entries={name:[entry for entry in state['entries'] if entry.get('text')==text and entry.get('origin')==origin] for name,state in states.items()}
        if any(len(rows)>1 for rows in entries.values()):
            raise AcceptanceError('UI Enter produced duplicate chat entries')
        other='client-1' if peer=='host' else 'host'
        if len(entries[peer])==len(entries[other])==1:
            sender,received=entries[peer][0],entries[other][0]
            required=2 if peer=='host' else 1
            if sender.get('delivery')=='confirmed' and sender.get('ack_peer_mask',0)&required==required and received.get('delivery')=='received':
                if sender.get('sequence')!=received.get('sequence'):
                    raise AcceptanceError('UI chat sequence differs between peers')
                runner.checked(peer+'-ui-delivery',entries)
                return
    raise AcceptanceError('UI Enter message recipient not confirmed; no resend')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup',type=Path,required=True)
    parser.add_argument('--freeze',type=Path,required=True)
    parser.add_argument('--wait-seconds',type=int,default=30,choices=range(10,61))
    args=parser.parse_args()
    args.test_root,args.players,args.cleanup,args.step_timeout=ROOT/'_MPTest',2,False,45
    runner=AcceptanceRunner(args)
    runner.summary['scope']='actual SDL/ImGui chat input, recipient confirmation and post-close keyboard forwarding on two owned peers'
    try:
        preflight(runner,args.freeze,args.setup,require_world=False)
        for peer,origin in (('client-1',2),('host',1)):
            initial=runner.single(peer+'-ui-before',peer,'game_read_chat_ui',{})
            if initial.get('visible') is not False or initial.get('typing') is not False or initial.get('text_pending') is not False:
                raise AcceptanceError('chat UI must start closed without pending text: '+peer)
            queued=runner.single(peer+'-f9-once',peer,'game_native_chat_ui_key',{'key':'F9'})
            if queued.get('state')!='queued':
                raise AcceptanceError('F9 queue admission absent')
            opened=observe(runner,peer,peer+'-actual-open',lambda s:s.get('visible') is True and s.get('rendered_visible') is True and s.get('input_active') is True and s.get('typing') is True and s.get('sdl_text_input_active') is True and s.get('f9_consumed')==initial['f9_consumed']+1,args.wait_seconds)
            if opened.get('draft')!='':
                raise AcceptanceError('chat UI draft must be empty before input')
            text='Koop prüfen - König '+runner.id+' '+peer
            runner.single(peer+'-text-once',peer,'game_native_chat_ui_text',{'text':text})
            typed=observe(runner,peer,peer+'-actual-text',lambda s:s.get('draft')==text and s.get('text_pending') is False and s.get('text_captured')==opened['text_captured']+1 and s.get('rendered_frames',0)>opened['rendered_frames'],args.wait_seconds)
            runner.single(peer+'-enter-once',peer,'game_native_chat_ui_key',{'key':'ENTER'})
            sent=observe(runner,peer,peer+'-actual-enter',lambda s:s.get('draft')=='' and s.get('input_active') is True and s.get('enter_captured')==typed['enter_captured']+1,args.wait_seconds)
            recipient(runner,peer,text,origin,args.wait_seconds)
            runner.single(peer+'-ui-frame',peer,'game_capture_frame',{})
            runner.single(peer+'-escape-once',peer,'game_native_chat_ui_key',{'key':'ESCAPE'})
            closed=observe(runner,peer,peer+'-actual-close',lambda s:s.get('visible') is False and s.get('typing') is False and s.get('rendered_visible') is False and s.get('sdl_text_input_active') is False and s.get('escape_consumed')==sent['escape_consumed']+1,args.wait_seconds)
            if closed['relative_mouse_mode']!=initial['relative_mouse_mode']:
                raise AcceptanceError('actual relative mouse mode was not restored: '+peer)
            runner.single(peer+'-forwarding-probe-once',peer,'game_native_chat_ui_key',{'key':'PROBE'})
            observe(runner,peer,peer+'-input-forwarded',lambda s:s.get('probe_forwarded')==closed['probe_forwarded']+1 and s.get('hook_overlay_active') is False,args.wait_seconds)
        runner.summary.update(passed=True,state='actual-two-peer-chat-ui-confirmed')
    except Exception as cause:
        log('error',str(cause))
        runner.summary.update(passed=False,state='failed-chat-ui',error=str(cause),input_replay_permitted=False)
    runner.summary['finished_utc']=utc()
    runner.persist()
    print(runner.summary_path)
    return 0 if runner.summary['passed'] else 1

if __name__=='__main__':
    raise SystemExit(main())
