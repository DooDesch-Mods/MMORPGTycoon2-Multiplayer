from pathlib import Path
import importlib.util
import sys
import unittest
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/mcp'))
import runtime
spec=importlib.util.spec_from_file_location('gm_policy_ui_server',ROOT/'tools/mcp/server.py')
server=importlib.util.module_from_spec(spec);sys.modules[spec.name]=server;spec.loader.exec_module(server)
class Fake:
    def __init__(self):self.calls=[]
    def available(self):return {'read_native_gm_policy_ui','native_gm_policy_ui'}
    def request(self,op,args):self.calls.append((op,args));return {'read_only':op.startswith('read_'),'execution_ack':False}
def args(command='ToggleCheckLog'):
    return dict(command=command,token='22222222-2222-4222-8222-222222222222',snapshot_uuid='11111111-1111-4111-8111-111111111111',epoch=3,generation=4,expected_value=False)
class PolicyUi(unittest.TestCase):
    def test_original_commands_and_read_only_boundary(self):
        fake=Fake();self.assertTrue(server.dispatch(fake,'game_read_native_gm_policy_ui',{})['read_only'])
        for command in ('SelectPolicy','ToggleCheckLog','ToggleChatEnabled'):
            value=args(command);self.assertFalse(server.dispatch(fake,'game_native_gm_policy_ui',value)['execution_ack'])
            self.assertEqual(fake.calls[-1],('native_gm_policy_ui',value))
        tools={tool.name:tool for tool in server.tools_for(fake)}
        self.assertTrue(tools['game_read_native_gm_policy_ui'].annotations.readOnlyHint)
        self.assertTrue(tools['game_native_gm_policy_ui'].annotations.destructiveHint)
        self.assertEqual(tools['game_native_gm_policy_ui'].inputSchema['type'],'object')
        self.assertIn('native_gm_policy_ui',runtime.MUTATIONS)
        self.assertNotIn('read_native_gm_policy_ui',runtime.MUTATIONS)
    def test_invalid_never_dispatches(self):
        invalid=[{},dict(args(),extra=True)]
        for key,value in [('command','SetHidden'),('command','Read'),('token','bad'),('epoch',True),('generation',0),('expected_value',1)]:invalid.append(dict(args(),**{key:value}))
        for value in invalid:
            fake=Fake()
            with self.subTest(value=value),self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(fake,'game_native_gm_policy_ui',value)
            self.assertEqual(fake.calls,[])
        with self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(Fake(),'game_read_native_gm_policy_ui',args())
    def test_all_offered_policy_fields(self):
        import release_ops
        for policy in ('chat','cheat'):
            for field,maximum in release_ops.POLICY_FIELDS.items():
                value=dict(args('SetPolicy'),policy=policy,field=field,value=True if maximum is None else maximum,expected_value=False if maximum is None else 0)
                fake=Fake();server.dispatch(fake,'game_native_gm_policy_ui',value);self.assertEqual(fake.calls[-1],('native_gm_policy_ui',value))
                invalid=[dict(value,policy='other'),dict(value,field='hidden'),dict(value,field=[]),dict(value,value=value['expected_value']),dict(value,extra=True)]
                invalid += [dict(value,value=1)] if maximum is None else [dict(value,value=True),dict(value,value=-1),dict(value,value=maximum+1),dict(value,expected_value=False)]
                for item in invalid:
                    rejected=Fake()
                    with self.subTest(item=item),self.assertRaises(runtime.RuntimeErrorDetail):server.dispatch(rejected,'game_native_gm_policy_ui',item)
                    self.assertEqual(rejected.calls,[])
    def test_unknown_is_preserved(self):
        class Failed(Fake):
            def request(self,op,args):raise runtime.RuntimeErrorDetail('native_error','GM policy original queue survivors/assignments/Lounge/spatial removal differs from cause')
        with self.assertRaisesRegex(runtime.RuntimeErrorDetail,'spatial removal differs'):server.dispatch(Failed(),'game_native_gm_policy_ui',args())
if __name__=='__main__':unittest.main()
