"""Validate the fixed service-sync route without invoking the game."""
from pathlib import Path
import importlib.util
import sys
import unittest

HERE=Path(__file__).resolve().parent
ROOT=next(p for p in [HERE,*HERE.parents] if (p/'tools/mcp/runtime.py').is_file())
sys.path.insert(0,str(ROOT/'tools/mcp'))
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module;spec.loader.exec_module(module);return module
runtime=load('runtime',ROOT/'tools/mcp/runtime.py')
server=load('service_sync_test_server',ROOT/'tools/mcp/server.py')
class Fake:
    def __init__(self):self.calls=[]
    def available(self):return {'read_native_service_sync_ui','native_service_sync_ui','native_service_select_ui'}
    def request(self,op,args):self.calls.append((op,args));return {'read_only':op.startswith('read_'),'execution_ack':False}
def args():return {'key':'action:building','ordinal':0,'token':'11111111-1111-4111-8111-111111111111','snapshot_uuid':'22222222-2222-4222-8222-222222222222','epoch':7,'generation':9,'expected_sync':False}
class ServiceSyncUI(unittest.TestCase):
    def test_dispatch(self):
        f=Fake();self.assertTrue(server.dispatch(f,'game_read_native_service_sync_ui',{'key':'action:building','ordinal':0})['read_only'])
        select={k:v for k,v in args().items() if k!='expected_sync'};self.assertFalse(server.dispatch(f,'game_native_service_select_ui',select)['execution_ack']);self.assertEqual(f.calls[-1],('native_service_select_ui',select))
        self.assertFalse(server.dispatch(f,'game_native_service_sync_ui',args())['execution_ack']);self.assertEqual(f.calls[-1],('native_service_sync_ui',args()))
        tools={tool.name:tool for tool in server.tools_for(f)};self.assertTrue(tools['game_read_native_service_sync_ui'].annotations.readOnlyHint);self.assertTrue(tools['game_native_service_sync_ui'].annotations.destructiveHint)
    def test_invalid_does_not_dispatch(self):
        values=[{}, {**args(),'command':'Toggle'}]
        for key,value in [('key',''),('key','bad\0key'),('ordinal',True),('ordinal',1024),('token','bad'),('epoch',0),('generation',2**64),('expected_sync',1)]:values.append({**args(),key:value})
        for value in values:
            f=Fake()
            with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(f,'game_native_service_sync_ui',value)
            self.assertEqual(f.calls,[])
if __name__=='__main__':unittest.main(verbosity=2)
