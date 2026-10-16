"""Check original Bug UI commands through SDK stdio and native acknowledgements."""
from copy import deepcopy
from unittest.mock import Mock
import unittest
import test_stdio_region as region_tests
from runtime import MUTATIONS, RuntimeErrorDetail
from server import dispatch, OP_SCHEMAS

TOKEN = '11111111-1111-4111-8111-111111111111'
ORIGIN = '22222222-2222-4222-8222-222222222222'
def arguments(command='Click'):
    return {'command':command,'token':TOKEN,'native_origin_uuid':ORIGIN,'epoch':1,'generation':1,
            'bug':[0,'action:'+TOKEN,1,TOKEN,0,1],'schedule':True}

class BugFixUiContract(unittest.TestCase):
    def test_read_is_not_a_mutation_and_click_is(self):
        self.assertNotIn('read_native_bug_fix_ui',MUTATIONS)
        self.assertIn('native_bug_fix_ui',MUTATIONS)
        self.assertEqual(set(OP_SCHEMAS['native_bug_fix_ui']['required']),set(arguments()))

    def test_invalid_arguments_cannot_reach_native(self):
        valid = arguments()
        invalid = [{},dict(valid,replay=True),dict(valid,command='MakeJob'),dict(valid,epoch=True),
                   dict(valid,generation=18446744073709551616),dict(valid,schedule=1),dict(valid,token=''),
                   dict(valid,bug=[0,'action:'+TOKEN,1,TOKEN,0]),dict(valid,bug=[False,'action:'+TOKEN,1,TOKEN,0,1]),
                   dict(valid,bug=[0,'action:'+TOKEN,1,TOKEN,0.0,1]),dict(valid,bug=[0,'action:'+TOKEN,2,TOKEN,0,1])]
        for value in invalid:
            runtime = Mock()
            with self.subTest(value=value), self.assertRaises(RuntimeErrorDetail):
                dispatch(runtime,'game_native_bug_fix_ui',value)
            runtime.request.assert_not_called()
        runtime = Mock()
        with self.assertRaises(RuntimeErrorDetail):
            dispatch(runtime,'game_read_native_bug_fix_ui',{'known':True})
        runtime.request.assert_not_called()

    def test_read_preserves_actual_readiness_cause(self):
        region_tests.RegionStdioTests().check_stdio('read_native_bug_fix_ui',{},({'known':True},),
            {'ready':False,'cause':'current Development detail view is absent','bugs':[]})

    def test_original_phase_commands_reach_native_once(self):
        for phase in ('Show','Select','Click'):
            with self.subTest(phase=phase):
                region_tests.RegionStdioTests().check_stdio('native_bug_fix_ui',arguments(phase),
                    (dict(arguments(phase),local_id=7),),
                    {'command':phase,'attempted':True,'returned':True,'confirmed':True,'capture_uuid':TOKEN if phase=='Click' else ''})

    def test_native_unknown_and_original_failure_are_retained(self):
        region_tests.RegionStdioTests().check_stdio('native_bug_fix_ui',arguments(),(),
            {'attempted':True,'returned':False,'confirmed':False,'unknown':True,'capture_uuid':TOKEN},
            native_error={'code':'BUG_FIX_UI_REJECTED','message':'original BugButton release returned without a capture'})

    def test_uint64_and_birth_kinds_preserve_identity(self):
        for kind,source,sequence in ((0,'a'*64,0),(1,TOKEN,0),(2,TOKEN,18446744073709551615)):
            value = deepcopy(arguments())
            value['bug'][2:] = [kind,source,sequence,18446744073709551615]
            value['epoch'] = value['generation'] = 18446744073709551615
            value['schedule'] = False
            runtime = Mock()
            dispatch(runtime,'game_native_bug_fix_ui',value)
            runtime.request.assert_called_once_with('native_bug_fix_ui',value)

if __name__=='__main__':
    unittest.main()
