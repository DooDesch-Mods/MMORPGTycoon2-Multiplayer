"""Check the bounded campaign UI route without invoking the game."""
from pathlib import Path
import importlib.util
import sys
import unittest

HERE = Path(__file__).resolve().parent
ROOT = next(p for p in [HERE, *HERE.parents] if (p / 'tools/mcp/runtime.py').is_file())
sys.path.insert(0, str(ROOT / 'tools/mcp'))
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module
runtime = load('runtime', ROOT / 'tools/mcp/runtime.py')
server = load('campaign_ui_test_server', ROOT / 'tools/mcp/server.py')

class Fake:
    def __init__(self): self.calls = []
    def available(self): return {'native_campaign_ui', 'read_native_campaign_ui'}
    def request(self, op, args):
        self.calls.append((op, args))
        return {'read_only': op.startswith('read_'), 'execution_ack': False}

def args():
    return {'command':'Start', 'token':'11111111-1111-4111-8111-111111111111',
            'snapshot_uuid':'22222222-2222-4222-8222-222222222222', 'epoch':7,
            'generation':9, 'expected_type':0}

class CampaignUI(unittest.TestCase):
    def test_dispatch_and_annotations(self):
        fake=Fake()
        self.assertTrue(server.dispatch(fake,'game_read_native_campaign_ui',{})['read_only'])
        for command in ('Show','SelectAdvertise','SelectCampaign','Start'):
            value={**args(),'command':command,'expected_type':0 if command in ('SelectCampaign','Start') else -1}
            self.assertFalse(server.dispatch(fake,'game_native_campaign_ui',value)['execution_ack'])
            self.assertEqual(fake.calls[-1],('native_campaign_ui',value))
        self.assertEqual(len(fake.calls),5)
        tools={tool.name:tool for tool in server.tools_for(fake)}
        self.assertTrue(tools['game_read_native_campaign_ui'].annotations.readOnlyHint)
        self.assertTrue(tools['game_native_campaign_ui'].annotations.destructiveHint)

    def test_invalid_never_dispatches(self):
        variants=[{}, {**args(),'extra':1}]
        for key,value in [('command','Read'),('token','bad'),('epoch',True),('epoch',0),
                          ('generation',2**64),('expected_type',True),('expected_type',-1),('expected_type',9)]:
            variants.append({**args(),key:value})
        variants.append({**args(),'command':'Show'})
        variants.append({**args(),'command':'SelectCampaign','expected_type':-1})
        variants.append({**args(),'command':'SelectCampaign','expected_type':9})
        for value in variants:
            fake=Fake()
            with self.assertRaises(runtime.RuntimeErrorDetail): server.dispatch(fake,'game_native_campaign_ui',value)
            self.assertEqual(fake.calls,[])

if __name__=='__main__': unittest.main(verbosity=2)
