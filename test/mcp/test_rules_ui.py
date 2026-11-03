import importlib.util
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mcp'))
import release_ops
from runtime import MUTATIONS, OPS, RuntimeErrorDetail
spec=importlib.util.spec_from_file_location('rules_ui_test_server',ROOT/'tools/mcp/server.py')
server=importlib.util.module_from_spec(spec)
sys.modules[spec.name]=server
spec.loader.exec_module(server)

class Fake:
    def __init__(self):self.calls=[]
    def available(self):return {'native_rules_ui','read_native_rules_ui'}
    def request(self,op,args):
        self.calls.append((op,args))
        return {'read_only':op.startswith('read_'),'execution_ack':False}

class RulesUi(unittest.TestCase):
    def args(self):
        return dict(command='ToggleDelay',token='22222222-2222-4222-8222-222222222222',
                    snapshot_uuid='11111111-1111-4111-8111-111111111111',epoch=3,generation=4,expected_delay=False)

    def test_strict_original_control_contract(self):
        self.assertIn('native_rules_ui',OPS)
        self.assertIn('read_native_rules_ui',OPS)
        self.assertIn('native_rules_ui',MUTATIONS)
        self.assertNotIn('read_native_rules_ui',MUTATIONS)
        release_ops.validate('read_native_rules_ui',{})
        for command in ('Show','ToggleDelay'):
            args=self.args();args['command']=command
            release_ops.validate('native_rules_ui',args)
        for field,value in (('epoch',True),('epoch',0),('generation',-1),('generation',2**64),
                            ('expected_delay',0),('expected_delay','false'),('command','ApplyXP'),
                            ('token','new'),('snapshot_uuid','wrong')):
            args=self.args();args[field]=value
            with self.subTest(field=field,value=value),self.assertRaises(RuntimeErrorDetail):
                release_ops.validate('native_rules_ui',args)
        for field in self.args():
            args=self.args();del args[field]
            with self.subTest(missing=field),self.assertRaises(RuntimeErrorDetail):
                release_ops.validate('native_rules_ui',args)
        args=self.args();args['desired_delay']=True
        with self.assertRaises(RuntimeErrorDetail):release_ops.validate('native_rules_ui',args)
        with self.assertRaises(RuntimeErrorDetail):release_ops.validate('read_native_rules_ui',self.args())

    def test_dispatch_and_failure_preserve_attempt_boundary(self):
        fake=Fake()
        self.assertTrue(server.dispatch(fake,'game_read_native_rules_ui',{})['read_only'])
        for command in ('Show','ToggleDelay'):
            args=self.args();args['command']=command
            self.assertFalse(server.dispatch(fake,'game_native_rules_ui',args)['execution_ack'])
            self.assertEqual(fake.calls[-1],('native_rules_ui',args))
        self.assertEqual(len(fake.calls),3)
        bad=self.args();bad['expected_delay']=1
        with self.assertRaises(RuntimeErrorDetail):server.dispatch(fake,'game_native_rules_ui',bad)
        self.assertEqual(len(fake.calls),3)
        tools={tool.name:tool for tool in server.tools_for(fake)}
        self.assertTrue(tools['game_read_native_rules_ui'].annotations.readOnlyHint)
        self.assertTrue(tools['game_native_rules_ui'].annotations.destructiveHint)
        class Failed(Fake):
            def request(self,op,args):raise RuntimeErrorDetail('native_error','Design original callback Unknown')
        with self.assertRaisesRegex(RuntimeErrorDetail,'Design original callback Unknown'):
            server.dispatch(Failed(),'game_native_rules_ui',self.args())

if __name__=='__main__':unittest.main()
