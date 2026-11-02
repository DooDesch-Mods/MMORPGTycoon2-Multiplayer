"""Validate the fixed original service-price route without invoking the game."""
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
server=load('service_price_test_server',ROOT/'tools/mcp/server.py')
class Fake:
    def __init__(self):self.calls=[]
    def available(self):return {'native_service_price_ui'}
    def request(self,op,args):self.calls.append((op,args));return {'execution_ack':False}
def args():return {'key':'action:building','ordinal':0,'token':'11111111-1111-4111-8111-111111111111','snapshot_uuid':'22222222-2222-4222-8222-222222222222','epoch':7,'generation':9,'command':'Increment'}
class ServicePriceUI(unittest.TestCase):
    def test_dispatch(self):
        for direction in ('Increment','Decrement'):
            f=Fake();value={**args(),'command':direction};self.assertFalse(server.dispatch(f,'game_native_service_price_ui',value)['execution_ack']);self.assertEqual(f.calls,[('native_service_price_ui',value)])
        tools={tool.name:tool for tool in server.tools_for(f)};self.assertTrue(tools['game_native_service_price_ui'].annotations.destructiveHint)
    def test_invalid_does_not_dispatch(self):
        for patch in ({'command':'SetPrice'},{'ordinal':True},{'epoch':0},{'key':'bad\0key'},{'amount':5}):
            f=Fake()
            with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(f,'game_native_service_price_ui',{**args(),**patch})
            self.assertEqual(f.calls,[])
if __name__=='__main__':unittest.main(verbosity=2)
