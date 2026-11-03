from pathlib import Path
import importlib.util
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mcp'))
import runtime,release_ops
spec=importlib.util.spec_from_file_location('numeric_rules_server',ROOT/'tools/mcp/server.py')
server=importlib.util.module_from_spec(spec);sys.modules[spec.name]=server;spec.loader.exec_module(server)
class Fake:
    def __init__(self):self.calls=[]
    def available(self):return {'native_rules_numeric_ui','read_native_rules_numeric_ui'}
    def request(self,op,args):self.calls.append((op,args));return {'read_only':op.startswith('read_'),'execution_ack':False}
def args(command='Seconds'):
    return dict(command=command,token='22222222-2222-4222-8222-222222222222',snapshot_uuid='11111111-1111-4111-8111-111111111111',epoch=3,generation=4,expected=10,value=11)
class NumericRules(unittest.TestCase):
    def test_offered_commands_and_exact_dispatch(self):
        fake=Fake();self.assertTrue(server.dispatch(fake,'game_read_native_rules_numeric_ui',{})['read_only'])
        for command in ('ShowCombat','ShowLevels','Seconds','XpPerMob','ApplyXP'):
            value=args(command)
            if command not in ('Seconds','XpPerMob'):value.update(expected=-1,value=-1)
            self.assertFalse(server.dispatch(fake,'game_native_rules_numeric_ui',value)['execution_ack'])
            self.assertEqual(fake.calls[-1],('native_rules_numeric_ui',value))
        self.assertEqual(len(fake.calls),6)
        tools={tool.name:tool for tool in server.tools_for(fake)}
        self.assertTrue(tools['game_read_native_rules_numeric_ui'].annotations.readOnlyHint)
        self.assertTrue(tools['game_native_rules_numeric_ui'].annotations.destructiveHint)
        self.assertIn('native_rules_numeric_ui',runtime.MUTATIONS)
        self.assertNotIn('read_native_rules_numeric_ui',runtime.MUTATIONS)
    def test_invalid_input_never_dispatches(self):
        values=[{},dict(args(),extra=1),dict(args(),value=10)]
        for key,value in [('command','Features'),('command','Read'),('token','bad'),('epoch',True),('generation',0),('generation',2**64),('expected',True),('value',-1),('value',2**31),('value','11')]:values.append({**args(),key:value})
        for value in values:
            fake=Fake()
            with self.subTest(value=value),self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_native_rules_numeric_ui',value)
            self.assertEqual(fake.calls,[])
        with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(Fake(),'game_read_native_rules_numeric_ui',args())
    def test_unknown_native_cause_remains_visible(self):
        class Failed(Fake):
            def request(self,op,args):raise runtime.RuntimeErrorDetail('native_error','original LostFocus commit Unknown')
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail,'original LostFocus commit Unknown'):server.dispatch(Failed(),'game_native_rules_numeric_ui',args())
if __name__=='__main__':unittest.main()
