from pathlib import Path
import argparse,copy,importlib.util,sys,unittest
sys.dont_write_bytecode=True
from tools.e2e import region_levels as levels
from unittest import mock
from test.e2e.test_region_properties import engine,state,cash

def args():return argparse.Namespace(region_id=67,min_level=3,max_level=5,setup=Path('offline-setup'),freeze=Path('offline-freeze'))
def actor(complete=False):return {'counts':[2,1,3],'roster_sha256':'c'*64,'levels_sha256':('e' if complete else 'd')*64,'expected_sha256':'e'*64,'global_random':[101,102,103,104] if complete else [1,2,3,4]}
def fixture():
    before={peer:engine(peer) for peer in levels.PEERS};after={peer:engine(peer,True) for peer in levels.PEERS};regions={peer:{**state(),'position_resolves_region':True,'region_position':[7000,5,9000],'actors':actor()} for peer in levels.PEERS};final=copy.deepcopy(regions)
    for peer in levels.PEERS:
        before[peer]['typed_actions']['attempted']=0;after[peer]['typed_actions']['attempted']=1
        final[peer].update(level=3,max_level=5,actors=actor(True));row=after[peer]['ui_region']['property_operations'][0]
        row.update(kind=4,before_levels=[1,3],after_levels=[3,5],level_brush=1000 if peer=='host' else 2000,level_graph=3000 if peer=='host' else 4000,level_constructed=True,level_destroyed=True,undo_confirmed=True,cause='',removed_uids=[],retired_shared_keys=[],retired_keys_sha256='4f53cda18c2baa0c0354bb5f9a3ecbe5ed12ab4d8e11ba873c2f11161202b945',level_before=actor(),level_after=actor(True),before_random=[1,2,3,4],after_random=[1,2,3,4],before_type_random=[5,6,7,8],after_type_random=[5,6,7,8])
    return before,regions,after,final
class Acceptance(unittest.TestCase):
    def test_success_uses_synchronous_rng_and_observations_can_differ(self):
        before,regions,after,final=fixture();levels.baseline(args(),before,regions)
        final['client-1']['actors']['global_random']=[201,202,203,204]
        result=levels.confirmed(args(),before,regions,cash(),after,final,'action');self.assertEqual(result['host_ack_mask'],3);self.assertNotEqual(result['global_rng_observations']['host'],result['global_rng_observations']['client-1'])
    def test_original_rng_mismatch_fails(self):
        before,regions,after,final=fixture();after['client-1']['ui_region']['property_operations'][0]['level_after']['global_random'][0]+=1
        with self.assertRaisesRegex(levels.AcceptanceError,'synchronous original receipts differ'):levels.confirmed(args(),before,regions,cash(),after,final,'action')
    def test_actor_precondition_and_baseline_owned_region(self):
        for field,value in [('roster_sha256','f'*64),('levels_sha256','f'*64),('counts',[3,1,3])]:
            before,regions,_,_=fixture();regions['client-1']['actors'][field]=value
            with self.subTest(field=field),self.assertRaisesRegex(levels.AcceptanceError,'canonical actor'):levels.baseline(args(),before,regions)
        before,regions,_,_=fixture()
        for value in regions.values():value['owned']=False
        with self.assertRaises(levels.AcceptanceError):levels.baseline(args(),before,regions)
    def test_native_result_and_ack_guard_failures(self):
        for which in range(9):
            before,regions,after,final=fixture();row=after['client-1']['ui_region']['property_operations'][0]
            if which==0:row['level_after']['levels_sha256']='f'*64
            if which==1:row['level_after']['counts'][0]+=1
            if which==2:row['undo_confirmed']=False
            if which==3:row['after_levels']=[3,6]
            if which==4:after['host']['typed_actions']['ack_mask']=1
            if which==5:final['client-1']['random']=[9,9,9,9]
            if which==6:after['client-1']['simulation']['line_income'][19]+=1
            if which==7:row['level_graph']=0
            if which==8:row['level_after']['expected_sha256']='f'*64
            with self.subTest(which=which),self.assertRaises(levels.AcceptanceError):levels.confirmed(args(),before,regions,cash(),after,final,'action')
    def test_admission(self):
        value={'attempted':4,'returned':4,'captured_once':True,'input_restored':True,'local_state_unchanged':True};levels.admission(value)
        for key in value:
            with self.subTest(key=key),self.assertRaises(levels.AcceptanceError):levels.admission({**value,key:False})
    def test_execute_never_replays_failed_mutation_or_timeout(self):
        before,regions,_,_=fixture();calls=[];roots=[];selected=[]
        class FakeRunner:
            def __init__(self,options):roots.append(options.test_root);self.summary={'step':'offline'};self.summary_path='offline-summary'
            def inspect(self,label,peer):return before[peer]
            def single(self,label,peer,tool,fields):
                calls.append(tool)
                if tool=='game_read_region_preset':return regions[peer]
                raise levels.AcceptanceError('fixture original UI timed out after attempted input')
            def checked(self,*_):pass
            def persist(self):pass
        prior_preflight,prior_identity=levels.preflight,levels.check_executable_identity
        try:
            levels.preflight=lambda *_,**kwargs:selected.append(kwargs['candidate_binary']);levels.check_executable_identity=lambda *_:None
            options=args();options.test_root=Path('owned-test-pair');options.candidate_binary=Path('frozen/MT2MultiplayerPlugin.dll')
            self.assertEqual(levels.execute(options,FakeRunner),1);self.assertEqual(calls.count('game_native_region_property_ui'),1)
            self.assertEqual(roots,[Path('owned-test-pair')])
            self.assertEqual(selected,[options.candidate_binary])
        finally:levels.preflight,levels.check_executable_identity=prior_preflight,prior_identity
    def test_cli_preserves_explicit_framework_candidate_and_legacy_default(self):
        command=['--setup','setup.json','--freeze','freeze.json','--region-id','67','--min-level','3','--max-level','5']
        for extra,expected in [([],None),(['--candidate-binary','frozen/MT2MultiplayerPlugin.dll'],Path('frozen/MT2MultiplayerPlugin.dll'))]:
            with self.subTest(candidate=expected),mock.patch.object(levels,'execute',return_value=0) as execute:
                self.assertEqual(levels.main(command+extra),0)
                self.assertEqual(execute.call_args.args[0].candidate_binary,expected)
if __name__=='__main__':unittest.main()
