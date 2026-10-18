"""Offline Framework SDK reader races. No game, mailbox or process is accessed."""
from pathlib import Path
import importlib.util
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

HERE=Path(__file__).resolve().parent
for parent in HERE.parents:
    if (parent/'tools/local_acceptance.py').is_file():
        ROOT=parent;break
sys.path[:0]=[str(ROOT/'tools'),str(ROOT/'tools/mcp')]
TOOL=HERE.parents[1]/'tools/lobby_ui/framework_acceptance.py'
spec=importlib.util.spec_from_file_location('framework_sdk_race_candidate',TOOL)
driver=importlib.util.module_from_spec(spec);spec.loader.exec_module(driver)
from runtime import RuntimeErrorDetail


class Clock:
    def __init__(self):self.now=0.0
    def monotonic(self):return self.now
    def sleep(self,seconds):self.now+=seconds


class FrameworkReadRaceTests(unittest.TestCase):
    def setUp(self):
        fixtures=HERE/'.fixtures';fixtures.mkdir(exist_ok=True)
        self.temporary=tempfile.TemporaryDirectory(dir=fixtures);self.addCleanup(self.temporary.cleanup)
        self.folder=Path(self.temporary.name)
        self.mailbox=self.folder/'synthetic-sdk';self.mailbox.mkdir()
        self.pending=self.mailbox/'request.json';self.response=self.mailbox/'response.json'
        self.response.write_text('{}',encoding='utf-8')
        self.clock=Clock()
        self.runner=SimpleNamespace(directory=self.folder/'evidence',args=SimpleNamespace(wait_seconds=.3),checked=mock.Mock())
        self.ui=driver.FrameworkUI.__new__(driver.FrameworkUI)
        self.ui.runner=self.runner;self.ui.peer='host';self.ui.directory=self.mailbox
        self.ui.identity={'pid':1234,'process_start_time':'133456789012345678'}
        self.ui.runtime=mock.Mock()
        self.command=None;self.writes=[];self.sequence=[];self.response_reads=0;self.pending_reads=0
        self.actions=[];self.pending_actions=[]
        self.default_response=lambda:self.ack()
        self.real_write=driver.write_json
        self.logs=[]
        for patch in (mock.patch.object(driver.time,'monotonic',self.clock.monotonic),
                      mock.patch.object(driver.time,'sleep',self.clock.sleep),
                      mock.patch.object(driver,'read_json',self.read),
                      mock.patch.object(driver,'write_json',self.write),
                      mock.patch.object(driver,'log',lambda level,message:self.logs.append((level,message)))):
            patch.start();self.addCleanup(patch.stop)

    def ack(self,command=None,**changes):
        command=command or self.command
        value={key:command[key] for key in ('id','pid','process_start_time')}
        value.update(ok=True,result={'launches':2,'panel_open':True});value.update(changes);return value

    def read(self,path):
        path=Path(path)
        if path==self.pending:
            self.pending_reads+=1
            action=self.pending_actions.pop(0) if self.pending_actions else json.loads(self.pending.read_text(encoding='utf-8'))
        elif path==self.response:
            self.response_reads+=1
            action=self.actions.pop(0) if self.actions else self.default_response
        else:raise AssertionError('Reader escaped its synthetic SDK fixture: '+str(path))
        self.sequence.append(('read',path))
        if callable(action):action=action()
        if isinstance(action,Exception):raise action
        return action

    def write(self,path,value):
        path=Path(path);self.sequence.append(('write',path));self.writes.append((path,json.loads(json.dumps(value))))
        if path==self.pending:self.command=dict(value)
        self.real_write(path,value)

    def previous(self):
        command={'id':'50000000-0000-4000-8000-000000000001',**self.ui.identity,
                 'operation':'desktop_activate','arguments':{'target':'start_mods'}}
        self.pending.write_text(json.dumps(command),encoding='utf-8');return command

    def request(self):return self.ui.request('desktop_activate',{'target':'start_mods'})
    def publications(self):return [value for path,value in self.writes if path==self.pending]
    def evidence(self):return [value for path,value in self.writes if path!=self.pending][-1]
    def partial(self):return json.JSONDecodeError('fixture partial native write','{"id":',6)

    def test_locked_response_recovers_without_republishing(self):
        self.actions=[PermissionError('fixture native writer lock'),lambda:self.ack()]
        self.assertEqual(self.request(),{'launches':2,'panel_open':True})
        self.assertEqual(len(self.publications()),1)
        receipt=self.evidence()['read_observations'][str(self.response)]
        self.assertEqual(receipt['failures'],1);self.assertEqual(receipt['recoveries'],1)
        self.assertIn('native writer lock',receipt['first_cause'])
        self.assertTrue(any(level=='warn' and 'observing only' in message for level,message in self.logs))
        self.assertTrue(any(level=='info' and 'recovered' in message for level,message in self.logs))
        self.assertGreaterEqual(self.ui.runtime._current.call_count,4)
        self.runner.checked.assert_called_once()

    def test_partial_json_recovers_without_republishing(self):
        self.actions=[self.partial(),lambda:self.ack()]
        self.request();self.assertEqual(len(self.publications()),1)
        self.assertIn('JSONDecodeError',self.evidence()['read_observations'][str(self.response)]['first_cause'])

    def test_first_cause_survives_different_later_read_failure(self):
        self.actions=[PermissionError('first sharing cause'),self.partial(),lambda:self.ack()]
        self.request();receipt=self.evidence()['read_observations'][str(self.response)]
        self.assertEqual(receipt['failures'],2)
        self.assertIn('first sharing cause',receipt['first_cause']);self.assertIn('JSONDecodeError',receipt['last_cause'])

    def test_persistent_response_lock_expires_retains_command_and_cause(self):
        self.default_response=lambda:PermissionError('persistent fixture lock')
        with self.assertRaises(driver.AcceptanceError) as caught:self.request()
        self.assertIn(str(self.response),str(caught.exception));self.assertIn('persistent fixture lock',str(caught.exception))
        self.assertIn('no replay',str(caught.exception));self.assertIsInstance(caught.exception.__cause__,PermissionError)
        self.assertEqual(len(self.publications()),1)
        self.assertEqual(json.loads(self.pending.read_text(encoding='utf-8')),self.command)
        self.assertEqual(self.evidence()['state'],'observation-timeout')
        self.assertGreater(self.evidence()['read_observations'][str(self.response)]['failures'],1)
        self.assertLessEqual(self.clock.now,.300001)
        self.assertEqual(sum(level=='warn' for level,_ in self.logs),1)
        self.runner.checked.assert_not_called()

    def test_persistent_partial_response_expires_with_json_cause(self):
        self.default_response=self.partial
        with self.assertRaises(driver.AcceptanceError) as caught:self.request()
        self.assertIsInstance(caught.exception.__cause__,json.JSONDecodeError)
        self.assertIn('JSONDecodeError',self.evidence()['error']);self.assertEqual(len(self.publications()),1)

    def test_previous_ack_lock_recovers_before_new_publication(self):
        previous=self.previous()
        self.actions=[PermissionError('previous response locked'),self.ack(previous),lambda:self.ack()]
        self.request();self.assertEqual(len(self.publications()),1)
        publication=self.sequence.index(('write',self.pending))
        self.assertGreaterEqual(sum(event==('read',self.response) for event in self.sequence[:publication]),2)
        self.assertIn('previous response locked',self.evidence()['read_observations'][str(self.response)]['first_cause'])

    def test_previous_request_partial_read_recovers_without_changing_old_ack_guard(self):
        previous=self.previous();self.pending_actions=[self.partial(),previous]
        self.actions=[self.ack(previous),lambda:self.ack()]
        self.request();self.assertEqual(self.pending_reads,2);self.assertEqual(len(self.publications()),1)
        self.assertEqual(self.evidence()['read_observations'][str(self.pending)]['recoveries'],1)

    def test_persistent_previous_ack_lock_never_publishes(self):
        self.previous();before=self.pending.read_bytes();self.default_response=lambda:PermissionError('old ACK blocked')
        with self.assertRaises(driver.AcceptanceError) as caught:self.request()
        self.assertIn('old ACK blocked',str(caught.exception));self.assertEqual(self.pending.read_bytes(),before)
        self.assertFalse(self.writes);self.assertIsNone(self.command);self.runner.checked.assert_not_called()

    def test_persistent_previous_request_partial_read_never_publishes(self):
        self.previous();before=self.pending.read_bytes();self.pending_actions=[self.partial()]*20
        with self.assertRaises(driver.AcceptanceError) as caught:self.request()
        self.assertIn(str(self.pending),str(caught.exception));self.assertEqual(self.pending.read_bytes(),before)
        self.assertFalse(self.writes);self.assertEqual(self.response_reads,0)

    def test_stable_previous_id_pid_and_start_mismatch_block_publication(self):
        for field,wrong in (('id','60000000-0000-4000-8000-000000000001'),('pid',9999),('process_start_time','other-start')):
            with self.subTest(field=field):
                previous=self.previous();before=self.pending.read_bytes();self.actions=[self.ack(previous,**{field:wrong})]
                with self.assertRaisesRegex(driver.AcceptanceError,'no exact acknowledgement'):self.request()
                self.assertFalse(self.publications());self.assertEqual(self.pending.read_bytes(),before)

    def test_current_response_pid_and_start_mismatch_never_become_success(self):
        for field,wrong in (('pid',9999),('process_start_time','other-start')):
            with self.subTest(field=field):
                self.actions=[lambda field=field,wrong=wrong:self.ack(**{field:wrong})]
                with self.assertRaisesRegex(driver.AcceptanceError,'process identity differs'):self.request()
                self.assertEqual(self.evidence()['state'],'identity-rejected')
                self.pending.unlink();self.writes.clear();self.command=None
        self.runner.checked.assert_not_called()

    def test_other_response_uuid_never_acknowledges_current_command(self):
        self.default_response=lambda:self.ack(id='60000000-0000-4000-8000-000000000001')
        with self.assertRaisesRegex(driver.AcceptanceError,'No exact SDK response'):self.request()
        self.assertEqual(len(self.publications()),1);self.runner.checked.assert_not_called()
        self.assertEqual(self.evidence()['state'],'observation-timeout')

    def test_native_explicit_failure_propagates_original_error(self):
        self.default_response=lambda:self.ack(ok=False,error='actual native handler failure')
        with self.assertRaisesRegex(driver.AcceptanceError,'actual native handler failure'):self.request()
        self.assertEqual(len(self.publications()),1);self.runner.checked.assert_not_called()
        self.assertEqual(self.evidence()['response']['ok'],False)

    def test_process_identity_change_during_retry_is_not_suppressed(self):
        def locked_then_reused():
            self.ui.runtime._current.side_effect=RuntimeErrorDetail('pid_reused','exact start token changed')
            return PermissionError('first transient sharing cause')
        self.default_response=locked_then_reused
        with self.assertRaisesRegex(RuntimeErrorDetail,'exact start token changed'):self.request()
        self.assertEqual(len(self.publications()),1);self.assertEqual(self.response_reads,1)
        self.assertTrue(any('first transient sharing cause' in message for _,message in self.logs))

    def test_previous_observation_and_response_poll_share_one_deadline(self):
        previous=self.previous();self.actions=[PermissionError('first old ACK lock'),self.partial(),self.ack(previous)]
        self.default_response=lambda:PermissionError('new response lock')
        with self.assertRaises(driver.AcceptanceError):self.request()
        self.assertEqual(len(self.publications()),1);self.assertAlmostEqual(self.clock.now,.3)

    def test_nontransient_read_failure_is_not_retried_or_hidden(self):
        self.default_response=lambda:OSError('permanent fixture read failure')
        with self.assertRaisesRegex(OSError,'permanent fixture read failure'):self.request()
        self.assertEqual(self.response_reads,1);self.assertEqual(len(self.publications()),1);self.assertEqual(self.clock.now,0)


if __name__=='__main__':unittest.main()
