"""Test fixed retirement SDK dispatch against actual staged server bodies."""
from pathlib import Path
import importlib.util
import sys
import unittest
HERE=Path(__file__).resolve().parent
SDK=HERE/'candidate/tools/mcp'
ROOT=HERE.parents[3] if SDK.is_dir() else HERE.parents[1]
if not SDK.is_dir():SDK=ROOT/'tools/mcp'
sys.path.insert(0,str(ROOT/'tools/mcp'))


def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);sys.modules[name]=module;spec.loader.exec_module(module);return module


runtime=load('runtime',SDK/'runtime.py')
ops=load('developer_staff_ops',SDK/'developer_staff_ops.py')
server=load('retirement_gate_test_server',SDK/'server.py')


class NoNativeRuntime:
    def __init__(self):self.calls=[]
    def available(self):return {'read_native_developer_retirement','native_developer_retirement_fixture'}
    def request(self,op,args):self.calls.append((op,args));return {'op':op,'args':args}


def request():return {'token':'11111111-1111-4111-8111-111111111111','native_origin_uuid':'22222222-2222-4222-8222-222222222222','epoch':1,'generation':1,'expected_count':1,'slot':1,'id':1}


class RetirementGate(unittest.TestCase):
    def test_exact_request_dispatch_once(self):
        fake=NoNativeRuntime();value=request();self.assertEqual(server.dispatch(fake,'game_native_developer_retirement_fixture',value)['args'],value);self.assertEqual(fake.calls,[('native_developer_retirement_fixture',value)])
    def test_read_empty_and_readonly(self):
        fake=NoNativeRuntime();server.dispatch(fake,'game_read_native_developer_retirement',{});self.assertEqual(fake.calls,[('read_native_developer_retirement',{})])
        with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_read_native_developer_retirement',{'slot':1})
        tools={t.name:t for t in server.tools_for(fake)}
        self.assertTrue(tools['game_read_native_developer_retirement'].annotations.readOnlyHint)
        self.assertFalse(tools['game_native_developer_retirement_fixture'].annotations.readOnlyHint)
        self.assertTrue(tools['game_native_developer_retirement_fixture'].annotations.destructiveHint)
    def test_invalid_never_dispatches(self):
        changes=[('token','bad'),('native_origin_uuid','bad'),('epoch',0),('epoch',True),('generation',2**64),('expected_count',51),('slot',-1),('slot',50),('slot',True),('id',-1),('id',50),('id',1.0)]
        for key,value in changes:
            with self.subTest(key=key,value=value):
                fake=NoNativeRuntime();args=request();args[key]=value
                with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_native_developer_retirement_fixture',args)
                self.assertEqual(fake.calls,[])
        for args in ({},{**request(),'command':'Fire'},{key:value for key,value in request().items() if key!='id'}):
            fake=NoNativeRuntime()
            with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_native_developer_retirement_fixture',args)
            self.assertEqual(fake.calls,[])
    def test_native_original_failure_preserved(self):
        class Failure(NoNativeRuntime):
            def request(self,op,args):raise runtime.RuntimeErrorDetail('native_error','original property Load rejected; no replay')
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail,'original property Load rejected; no replay'):server.dispatch(Failure(),'game_native_developer_retirement_fixture',request())


if __name__=='__main__':unittest.main(verbosity=2)
