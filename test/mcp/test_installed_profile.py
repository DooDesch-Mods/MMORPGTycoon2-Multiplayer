from pathlib import Path
import copy
import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
import uuid

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
spec=importlib.util.spec_from_file_location('runtime',ROOT/'tools/mcp/runtime.py')
runtime=importlib.util.module_from_spec(spec);sys.modules['runtime']=runtime;spec.loader.exec_module(runtime)

class InstalledProofTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=HERE);self.addCleanup(self.temp.cleanup)
        self.proof=Path(self.temp.name)/'_MPTest/dropin-proof';self.game=self.proof/'game';self.game.mkdir(parents=True)
        install_id=hashlib.sha256(str(self.game).lower().encode('utf-16le')).hexdigest()
        self.root=self.proof/'appdata/MT2Multiplayer'/install_id;(self.root/'data').mkdir(parents=True);(self.root/'diagnostics').mkdir()
        self.modules={}
        for name,filename in {'executable':'MT2.exe','loader':'WSOCK32.dll','mod':'MT2_Multiplayer.dll','steam':'steam_api64.dll'}.items():
            path=self.game/filename;path.write_bytes(name.encode());self.modules[name]={'path':str(path),'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        self.run=str(uuid.uuid4());self.meta={'acceptance_root':str(self.proof),'game_dir':str(self.game),'runtime_root':str(self.root),'user_data_dir':str(self.root/'data'),'diagnostic_mailbox_enabled':True,'local_test':False,'isolation_flags':3,'process_start_time':'123456','modules':self.modules}
        self.manifest={'schema_version':1,'profile':'installed_dropin_proof','run_id':self.run,'pid':333,'process_start_time':'123456','runtime_root':str(self.root),'executable_path':str(self.game/'MT2.exe'),'owner':{'kind':'MPTest','run_id':self.run},'installed_proof':self.meta}
        self.path=self.root/'instance.json';self.path.write_text(json.dumps(self.manifest),encoding='utf8')
        self.patch=patch.object(runtime,'INSTALLED_PROOF_ROOT',self.proof);self.patch.start();self.addCleanup(self.patch.stop)
        self.hashpatch=patch.object(runtime,'INSTALLED_EXE_SHA256',self.modules['executable']['sha256']);self.hashpatch.start();self.addCleanup(self.hashpatch.stop)
        self.process={key:self.manifest[key] for key in ['pid','process_start_time','executable_path']};self.process['owned_by_current_user']=True
        self.loaded=[item['path'] for item in self.modules.values()]
        self.runtime=runtime.Runtime(self.proof,inspector=lambda pid:self.process,module_inspector=lambda pid:self.loaded)
    def error(self,code,mutate):
        value=copy.deepcopy(self.manifest);mutate(value);self.path.write_text(json.dumps(value),encoding='utf8')
        with self.assertRaises(runtime.RuntimeErrorDetail) as error:self.runtime.bind(str(self.path))
        self.assertEqual(error.exception.code,code)
    def test_positive_real_steam_private_layout(self):
        self.assertTrue(self.runtime.bind(str(self.path))['process_identity_verified'])
        status={'schema_version':1,'run_id':self.run,'pid':333,'game_thread_ready':True,'capabilities':{'read_native_lobby':True},'installed_proof':self.meta}
        (self.root/'diagnostics/status.json').write_text(json.dumps(status),encoding='utf8')
        self.assertIn('read_native_lobby',self.runtime.available())
    def test_reject_local_test_fake(self):self.error('ownership_failed',lambda v:v['installed_proof'].update(local_test=True))
    def test_reject_disabled_native_policy(self):self.error('ownership_failed',lambda v:v['installed_proof'].update(diagnostic_mailbox_enabled=False))
    def test_reject_normal_user_root(self):self.error('path_escape',lambda v:v['installed_proof'].update(acceptance_root=str(self.proof.parent/'normal-user')))
    def test_reject_executable_outside_owned_game(self):self.error('ownership_failed',lambda v:v.update(executable_path=str(self.proof/'MT2.exe')))
    def test_reject_missing_loaded_loader(self):
        self.loaded.pop(1)
        with self.assertRaises(runtime.RuntimeErrorDetail) as error:self.runtime.bind(str(self.path))
        self.assertEqual(error.exception.code,'module_identity_failed')
    def test_reject_changed_mod(self):
        (self.game/'MT2_Multiplayer.dll').write_bytes(b'changed')
        self.error('module_changed',lambda v:None)
    def test_reject_reused_pid(self):
        self.process['process_start_time']='999999';self.error('pid_reused',lambda v:None)
    def test_reject_foreign_account(self):
        self.process['owned_by_current_user']=False;self.error('ownership_failed',lambda v:None)
    def test_status_requires_same_native_receipt(self):
        self.runtime.bind(str(self.path));status={'schema_version':1,'run_id':self.run,'pid':333,'game_thread_ready':True,'capabilities':{},'installed_proof':{}}
        (self.root/'diagnostics/status.json').write_text(json.dumps(status),encoding='utf8')
        with self.assertRaises(runtime.RuntimeErrorDetail) as error:self.runtime.status()
        self.assertEqual(error.exception.code,'stale_status')

if __name__=='__main__':
    unittest.main()
