"""Only bound status publication may wait briefly for Windows error2."""
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, patch
import uuid

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools/mcp"))
from runtime import Runtime,RuntimeErrorDetail,STATUS_PUBLICATION_WAIT_SECONDS,MAX_JSON,MAX_STATUS_JSON,atomic_json,read_json,shared_open
from server import dispatch


def missing(code=2):
    error=OSError(f"Windows status publication probe error {code}")
    error.winerror=code
    return error


class StatusPublicationTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=Path(__file__).parent)
        self.addCleanup(self.temp.cleanup)
        instances=Path(self.temp.name)/"_MPTest"
        self.root=instances/"host";self.diagnostics=self.root/"diagnostics"
        self.diagnostics.mkdir(parents=True)
        self.manifest={"schema_version":1,"run_id":str(uuid.uuid4()),"pid":4321,"process_start_time":"133400000000000001",
                       "runtime_root":str(self.root.resolve()),"executable_path":str((self.root/"game/MT2.exe").resolve())}
        self.manifest["owner"]={"kind":"MPTest","run_id":self.manifest["run_id"]}
        self.manifest_path=self.root/"instance.json";atomic_json(self.manifest_path,self.manifest)
        self.identity={"process_start_time":self.manifest["process_start_time"],"executable_path":self.manifest["executable_path"],"owned_by_current_user":True}
        self.inspector=Mock(return_value=self.identity)
        self.runtime=Runtime(instances,inspector=self.inspector,timeout=.1)
        self.runtime.bind(str(self.manifest_path));self.inspector.reset_mock()
        self.status={"schema_version":1,"run_id":self.manifest["run_id"],"pid":self.manifest["pid"],"game_thread_ready":True,"capabilities":{"inspect_engine":True,"set_time_scale":True}}

    def reader(self,results):
        statuses=iter(results)
        def read(path):
            if path.name!="status.json":return shared_open(path)
            value=next(statuses)
            if isinstance(value,Exception):raise value
            return io.BytesIO(value if isinstance(value,bytes) else json.dumps(value).encode())
        return read

    def test_status_recovery_rechecks_manifest_and_process_and_logs_once(self):
        with patch('runtime.shared_open',side_effect=self.reader([missing(),missing(),self.status])) as opened,patch.object(self.runtime,'_manifest',wraps=self.runtime._manifest) as manifests:
            with self.assertLogs('mt2_mcp',level='INFO') as logs:
                self.assertEqual(self.runtime.status(),self.status)
        self.assertEqual(self.inspector.call_count,4)
        self.assertEqual(manifests.call_count,4)
        self.assertEqual(sum(call.args[0].name=='status.json' for call in opened.call_args_list),3)
        self.assertEqual(len(logs.output),2)
        self.assertIn('publication wait target=',logs.output[0]);self.assertIn('failures=2',logs.output[-1])
        self.assertIn('publication recovered',logs.output[-1]);self.assertIn('original_cause=Windows status publication probe error 2',logs.output[-1])

    def test_persistent_missing_status_is_bounded_and_preserves_native_cause(self):
        failure=missing();original=shared_open
        def read(path):
            if path.name=='status.json':raise failure
            return original(path)
        started=time.monotonic()
        with patch('runtime.shared_open',side_effect=read):
            with self.assertLogs('mt2_mcp',level='INFO') as logs:
                with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        elapsed=time.monotonic()-started
        self.assertGreaterEqual(elapsed,STATUS_PUBLICATION_WAIT_SECONDS)
        self.assertLess(elapsed,STATUS_PUBLICATION_WAIT_SECONDS+.5)
        self.assertEqual(caught.exception.code,'read_failed');self.assertIs(caught.exception.__cause__,failure)
        self.assertEqual(len(logs.output),2);self.assertIn('wait exhausted',logs.output[-1]);self.assertIn('required_limit_ms=250.0',logs.output[-1])

    def test_changed_process_stops_before_recovered_status_is_read(self):
        changed=dict(self.identity,process_start_time='133400000000000002')
        self.inspector.side_effect=[self.identity,changed]
        with patch('runtime.shared_open',side_effect=self.reader([missing(),self.status])) as opened:
            with self.assertLogs('mt2_mcp',level='INFO') as logs:
                with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'pid_reused')
        self.assertEqual(sum(call.args[0].name=='status.json' for call in opened.call_args_list),1)
        self.assertIn('wait aborted',logs.output[-1]);self.assertIn('original_cause=',logs.output[-1])

    def test_changed_manifest_stops_before_recovered_status_is_read(self):
        reads=self.reader([missing(),self.status])
        def read(path):
            if path.name=='status.json':
                changed=dict(self.manifest,peer_id=9);atomic_json(self.manifest_path,changed)
            return reads(path)
        with patch('runtime.shared_open',side_effect=read) as opened:
            with self.assertLogs('mt2_mcp',level='INFO'):
                with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'manifest_changed')
        self.assertEqual(sum(call.args[0].name=='status.json' for call in opened.call_args_list),1)

    def test_stale_status_after_missing_is_rejected_without_recovery_log(self):
        stale=dict(self.status,run_id=str(uuid.uuid4()))
        with patch('runtime.shared_open',side_effect=self.reader([missing(),stale])):
            with self.assertLogs('mt2_mcp',level='INFO') as logs:
                with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'stale_status')
        self.assertFalse(any('publication recovered' in line for line in logs.output))

    def test_invalid_json_after_missing_is_not_retried(self):
        with patch('runtime.shared_open',side_effect=self.reader([missing(),b'{bad-json'])) as opened:
            with self.assertLogs('mt2_mcp',level='INFO') as logs:
                with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'read_failed');self.assertIsInstance(caught.exception.__cause__,json.JSONDecodeError)
        self.assertEqual(sum(call.args[0].name=='status.json' for call in opened.call_args_list),2)
        self.assertFalse(any('publication recovered' in line for line in logs.output))

    def test_other_status_io_errors_are_immediate(self):
        for code in (3,5):
            with self.subTest(code=code),patch('runtime.shared_open',side_effect=self.reader([missing(code)])) as opened,patch('runtime.time.sleep') as waited:
                with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
                self.assertEqual(caught.exception.code,'read_failed');waited.assert_not_called()
                self.assertEqual(sum(call.args[0].name=='status.json' for call in opened.call_args_list),1)

    def test_missing_manifest_is_not_status_publication_wait(self):
        with patch('runtime.shared_open',side_effect=missing()),patch('runtime.time.sleep') as waited:
            with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'read_failed');waited.assert_not_called()

    def test_process_changes_after_open_before_return_never_succeeds(self):
        changed=dict(self.identity,owned_by_current_user=False)
        self.inspector.side_effect=[self.identity,changed]
        with patch('runtime.shared_open',side_effect=self.reader([self.status])):
            with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'ownership_failed')

    def test_available_uses_recovered_verified_status(self):
        with patch('runtime.shared_open',side_effect=self.reader([missing(),self.status])):
            with self.assertLogs('mt2_mcp',level='INFO'):
                self.assertEqual(self.runtime.available(),{'inspect_engine','set_time_scale'})

    def test_invoke_failure_never_publishes_or_creates_mailbox_lock(self):
        changed=dict(self.identity,process_start_time='133400000000000002')
        self.inspector.side_effect=[self.identity,self.identity,changed]
        with patch('runtime.shared_open',side_effect=self.reader([missing(),self.status])),patch('runtime.atomic_json') as publish:
            with self.assertLogs('mt2_mcp',level='INFO'):
                with self.assertRaises(RuntimeErrorDetail) as caught:dispatch(self.runtime,'game_set_time_scale',{'scale':0})
        self.assertEqual(caught.exception.code,'pid_reused');publish.assert_not_called()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_invoke_recovery_publishes_exactly_once(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        with patch('runtime.shared_open',side_effect=self.reader([missing(),self.status])),patch('runtime.atomic_json',wraps=atomic_json) as publish,patch('runtime.uuid.uuid4',return_value=request_id),patch.object(self.runtime,'_matching_response',return_value=response):
            with self.assertLogs('mt2_mcp',level='INFO'):
                self.assertEqual(dispatch(self.runtime,'game_set_time_scale',{'scale':0}),response)
        publish.assert_called_once()
        self.assertEqual(publish.call_args.args[0].name,'request.json')
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def sized_json(self, value, size):
        result=dict(value,diagnostic_padding="")
        payload=json.dumps(result).encode()
        self.assertLessEqual(len(payload),size)
        result["diagnostic_padding"]="x"*(size-len(payload))
        payload=json.dumps(result).encode()
        self.assertEqual(len(payload),size)
        return payload

    def test_actual_status_size_and_engine_reply_are_accepted_once(self):
        payload=self.sized_json(self.status,400832)
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':json.loads(payload)}
        (self.diagnostics/'status.json').write_bytes(payload)
        (self.diagnostics/'response.json').write_text(json.dumps(response),encoding='utf8')
        with patch('runtime.uuid.uuid4',return_value=request_id),patch('runtime.atomic_json',wraps=atomic_json) as published:
            self.assertEqual(dispatch(self.runtime,'game_inspect_engine',{}),response)
        published.assert_called_once()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_large_native_lobby_response_is_accepted_once(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        self.status['capabilities']['read_native_lobby']=True
        atomic_json(self.diagnostics/'status.json',self.status)
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        payload=self.sized_json(response,400832)
        (self.diagnostics/'response.json').write_bytes(payload)
        with patch('runtime.uuid.uuid4',return_value=request_id),patch('runtime.atomic_json',wraps=atomic_json) as published:
            self.assertEqual(dispatch(self.runtime,'game_read_native_lobby',{}),json.loads(payload))
        published.assert_called_once()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_large_pending_native_lobby_response_observes_without_replay(self):
        request_id=str(uuid.uuid4())
        atomic_json(self.diagnostics/'request.lock',dict(self.manifest,id=request_id,op='read_native_lobby'))
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        payload=self.sized_json(response,400832)
        (self.diagnostics/'response.json').write_bytes(payload)
        with patch('runtime.atomic_json') as published:
            self.assertEqual(self.runtime.observe_pending(),{'pending':False,'response':json.loads(payload)})
        published.assert_not_called()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_native_lobby_above_ceiling_retains_error_and_lock(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        self.status['capabilities']['read_native_lobby']=True
        atomic_json(self.diagnostics/'status.json',self.status)
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        (self.diagnostics/'response.json').write_bytes(self.sized_json(response,MAX_STATUS_JSON+1))
        with patch('runtime.uuid.uuid4',return_value=request_id),patch('runtime.atomic_json',wraps=atomic_json) as published:
            with self.assertRaises(RuntimeErrorDetail) as caught:
                dispatch(self.runtime,'game_read_native_lobby',{})
        self.assertEqual(caught.exception.code,'oversize_json')
        self.assertIn(str(MAX_STATUS_JSON),str(caught.exception))
        self.assertTrue((self.diagnostics/'request.lock').exists())
        published.assert_called_once()
        with patch('runtime.atomic_json') as replay:
            with self.assertRaises(RuntimeErrorDetail) as caught:
                self.runtime.observe_pending()
        self.assertEqual(caught.exception.code,'oversize_json')
        replay.assert_not_called()
        self.assertTrue((self.diagnostics/'request.lock').exists())

    def test_large_camera_response_is_accepted_once(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        self.status['capabilities']['focus_test_camera']=True
        atomic_json(self.diagnostics/'status.json',self.status)
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{'state':'camera_focus_applied'}}
        payload=self.sized_json(response,509831)
        (self.diagnostics/'response.json').write_bytes(payload)
        with patch('runtime.uuid.uuid4',return_value=request_id),patch('runtime.atomic_json',wraps=atomic_json) as published:
            self.assertEqual(dispatch(self.runtime,'game_focus_test_camera',{'x':8020,'y':70,'z':8550,'distance':150}),json.loads(payload))
        published.assert_called_once()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_large_pending_camera_response_observes_without_replay(self):
        request_id=str(uuid.uuid4())
        atomic_json(self.diagnostics/'request.lock',dict(self.manifest,id=request_id,op='focus_test_camera'))
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{'state':'camera_focus_applied'}}
        payload=self.sized_json(response,509831)
        (self.diagnostics/'response.json').write_bytes(payload)
        with patch('runtime.atomic_json') as published:
            self.assertEqual(self.runtime.observe_pending(),{'pending':False,'response':json.loads(payload)})
        published.assert_not_called()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_camera_above_ceiling_retains_error_and_lock(self):
        request_id=str(uuid.uuid4())
        atomic_json(self.diagnostics/'request.lock',dict(self.manifest,id=request_id,op='focus_test_camera'))
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        (self.diagnostics/'response.json').write_bytes(self.sized_json(response,MAX_STATUS_JSON+1))
        with patch('runtime.atomic_json') as published:
            with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.observe_pending()
        self.assertEqual(caught.exception.code,'oversize_json')
        published.assert_not_called()
        self.assertTrue((self.diagnostics/'request.lock').exists())

    def test_status_ceiling_exact_and_one_byte_over_preserve_error(self):
        payload=self.sized_json(self.status,MAX_STATUS_JSON)
        with patch('runtime.shared_open',side_effect=self.reader([payload])):
            self.assertEqual(self.runtime.status()['pid'],self.status['pid'])
        with patch('runtime.shared_open',side_effect=self.reader([payload+b' '])) as opened,patch('runtime.time.sleep') as waited:
            with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'oversize_json')
        self.assertIn(str(MAX_STATUS_JSON),str(caught.exception))
        waited.assert_not_called()
        self.assertEqual(sum(call.args[0].name=='status.json' for call in opened.call_args_list),1)

    def test_large_status_keeps_exact_runtime_identity(self):
        payload=self.sized_json(dict(self.status,run_id=str(uuid.uuid4())),400832)
        with patch('runtime.shared_open',side_effect=self.reader([payload])):
            with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime.status()
        self.assertEqual(caught.exception.code,'stale_status')

    def test_other_json_inputs_and_response_operations_keep_old_limit(self):
        payload=self.sized_json(self.status,400832)
        for name in ('job.json','instance-copy.json','request.json','status.json'):
            with self.subTest(name=name):
                path=self.diagnostics/name;path.write_bytes(payload)
                with self.assertRaises(RuntimeErrorDetail) as caught:read_json(path)
                self.assertEqual(caught.exception.code,'oversize_json');self.assertIn(str(MAX_JSON),str(caught.exception))
        request_id=str(uuid.uuid4())
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':json.loads(payload)}
        (self.diagnostics/'response.json').write_text(json.dumps(response),encoding='utf8')
        with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime._matching_response(request_id,'read_native_developers')
        self.assertEqual(caught.exception.code,'oversize_json')
        with self.assertRaises(RuntimeErrorDetail) as caught:atomic_json(self.diagnostics/'write.json',json.loads(payload))
        self.assertEqual(caught.exception.code,'oversize_json')

    def test_pending_status_response_uses_recorded_operation_without_replay(self):
        request_id=str(uuid.uuid4());record=dict(self.manifest,id=request_id,op='inspect_engine')
        atomic_json(self.diagnostics/'request.lock',record)
        result=json.loads(self.sized_json(self.status,400832))
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':result}
        (self.diagnostics/'response.json').write_text(json.dumps(response),encoding='utf8')
        with patch('runtime.atomic_json') as published:
            observed=self.runtime.observe_pending()
        self.assertEqual(observed,{'pending':False,'response':response});published.assert_not_called()
        self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_oversize_status_response_keeps_pending_lock(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        atomic_json(self.diagnostics/'status.json',self.status)
        payload=self.sized_json({'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}},MAX_STATUS_JSON+1)
        (self.diagnostics/'response.json').write_bytes(payload)
        with patch('runtime.uuid.uuid4',return_value=request_id):
            with self.assertRaises(RuntimeErrorDetail) as caught:dispatch(self.runtime,'game_inspect_engine',{})
        self.assertEqual(caught.exception.code,'oversize_json')
        self.assertTrue((self.diagnostics/'request.lock').exists())

    def test_large_previous_status_does_not_block_next_small_response(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        self.status['capabilities']['read_building_history_ui']=True
        atomic_json(self.diagnostics/'status.json',self.status)
        stale={'schema_version':1,'id':str(uuid.uuid4()),'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        previous=self.sized_json(stale,400832)
        current={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{'confirmed':True}}
        (self.diagnostics/'response.json').write_bytes(previous)
        responses=iter([previous,json.dumps(current).encode()]);original=shared_open
        def read(path):
            return io.BytesIO(next(responses)) if path.name=='response.json' else original(path)
        with patch('runtime.uuid.uuid4',return_value=request_id),patch('runtime.shared_open',side_effect=read),patch('runtime.atomic_json',wraps=atomic_json) as published:
            self.assertEqual(dispatch(self.runtime,'game_read_building_history_ui',{}),current)
        published.assert_called_once();self.assertFalse((self.diagnostics/'request.lock').exists())

    def test_matching_large_ordinary_response_keeps_lock(self):
        request_id='01234567-89ab-cdef-0123-456789abcdef'
        self.status['capabilities']['read_building_history_ui']=True
        atomic_json(self.diagnostics/'status.json',self.status)
        response={'schema_version':1,'id':request_id,'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        (self.diagnostics/'response.json').write_bytes(self.sized_json(response,400832))
        with patch('runtime.uuid.uuid4',return_value=request_id):
            with self.assertRaises(RuntimeErrorDetail) as caught:dispatch(self.runtime,'game_read_building_history_ui',{})
        self.assertEqual(caught.exception.code,'oversize_json');self.assertIn(str(MAX_JSON),str(caught.exception))
        self.assertTrue((self.diagnostics/'request.lock').exists())

    def test_stale_response_above_outer_ceiling_still_fails(self):
        stale={'schema_version':1,'id':str(uuid.uuid4()),'run_id':self.manifest['run_id'],'ok':True,'result':{}}
        (self.diagnostics/'response.json').write_bytes(self.sized_json(stale,MAX_STATUS_JSON+1))
        with self.assertRaises(RuntimeErrorDetail) as caught:self.runtime._matching_response(str(uuid.uuid4()),'read_building_history_ui')
        self.assertEqual(caught.exception.code,'oversize_json');self.assertIn(str(MAX_STATUS_JSON),str(caught.exception))

    @unittest.skipUnless(os.name=='nt','real WinError2 publication fixture requires Windows')
    def test_real_win32_missing_status_then_atomic_publication_recovers(self):
        self.assertFalse((self.diagnostics/'status.json').exists())
        def publish(_seconds):atomic_json(self.diagnostics/'status.json',self.status)
        with patch('runtime.time.sleep',side_effect=publish) as waited:
            with self.assertLogs('mt2_mcp',level='INFO') as logs:
                self.assertEqual(self.runtime.status(),self.status)
        waited.assert_called_once()
        self.assertEqual(self.inspector.call_count,3)
        self.assertIn('[WinError 2]',logs.output[0]);self.assertIn('publication recovered',logs.output[-1])


if __name__=='__main__':unittest.main()
