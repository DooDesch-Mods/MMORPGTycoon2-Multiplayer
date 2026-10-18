"""Run actual acceptance entry points with owned files and explicit protocol fixtures."""
from pathlib import Path
import copy
import hashlib
import runpy
import sys
import tempfile
from types import ModuleType,SimpleNamespace
import unittest
from unittest.mock import Mock,patch
import uuid

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import local_acceptance as acceptance
from runtime import RuntimeErrorDetail,atomic_json,framework_module_paths


class FrameworkAcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=HERE);self.addCleanup(self.temp.cleanup)
        self.workspace=Path(self.temp.name);self.instances=self.workspace/"_MPTest";self.run=str(uuid.uuid4());self.identities={};self.paths={}
        for peer,name in ((1,"host"),(2,"client-1")):
            root=self.instances/name;root.mkdir(parents=True)
            value={"schema_version":2,"runtime_kind":"framework-plugin","framework_profile":str(root/"profile"),
                   "run_id":self.run,"pid":200+peer,"process_start_time":str(133400000000000000+peer),
                   "runtime_root":str(root),"executable_path":str(root/"game/MT2.exe"),"owner":{"kind":"MPTest","run_id":self.run},"peer_id":peer}
            modules=framework_module_paths(value)
            for filename,path in modules.items():path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(filename.encode())
            value["installed_binary_sha256"]={filename:hashlib.sha256(path.read_bytes()).hexdigest() for filename,path in modules.items()}
            self.identities[name]=value;self.paths[name]=root/"instance.json";atomic_json(self.paths[name],value)
        self.runner=object.__new__(acceptance.AcceptanceRunner);self.runner.args=SimpleNamespace(players=2);self.runner.root=self.instances
        self.runner.manifests={};self.runner.summary={};self.runner.checked=Mock()

    def legacy(self):
        for name,value in self.identities.items():
            value["schema_version"]=1;value.pop("runtime_kind");value.pop("framework_profile")
            dll=Path(value["runtime_root"])/"game/MT2_Multiplayer.dll";dll.write_bytes(b"legacy-fixture")
            value["installed_binary_sha256"]={"MT2_Multiplayer.dll":hashlib.sha256(dll.read_bytes()).hexdigest()};atomic_json(self.paths[name],value)

    def test_load_schema2_validates_actual_artifacts_for_both_peers(self):
        self.runner.load_manifests();self.assertEqual(self.runner.manifests,self.paths);self.assertEqual(self.runner.summary["runtime_run_id"],self.run)

    def test_load_legacy_branch_still_uses_legacy_manifest_contract(self):
        self.legacy();self.runner.load_manifests();self.assertEqual(self.runner.manifests,self.paths)

    def test_load_wrong_framework_artifact_or_mixed_mode_rejects(self):
        path=framework_module_paths(self.identities["client-1"])["MT2MultiplayerPlugin.dll"];path.write_bytes(b"changed")
        with self.assertRaisesRegex(RuntimeErrorDetail,"hash differs"):self.runner.load_manifests()
        path.write_bytes(b"MT2MultiplayerPlugin.dll")
        value=self.identities["client-1"];value["schema_version"]=1;value.pop("runtime_kind");value.pop("framework_profile");atomic_json(self.paths["client-1"],value)
        with self.assertRaisesRegex(acceptance.AcceptanceError,"standalone peers"):self.runner.load_manifests()

    def capture_fixture(self,path):
        path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b"frame-fixture")
        self.runner.manifests=self.paths;self.runner.single=Mock(return_value={"path":str(path),"bytes":path.stat().st_size,"width":16,"height":9})

    def test_framework_capture_accepts_only_multiplayer_subdirectory(self):
        root=Path(self.identities["host"]["runtime_root"])
        self.capture_fixture(root/"profile/diagnostics/multiplayer/frame.bmp");self.runner.capture("host")
        self.assertEqual(self.runner.checked.call_args.args[0],"native-frame-artifact")
        for path in (root/"diagnostics/frame.bmp",root/"profile/diagnostics/frame.bmp"):
            self.capture_fixture(path)
            with self.subTest(path=path),self.assertRaisesRegex(acceptance.AcceptanceError,"outside root"):self.runner.capture("host")

    def test_legacy_capture_boundary_is_unchanged(self):
        self.legacy();root=Path(self.identities["host"]["runtime_root"])
        self.capture_fixture(root/"diagnostics/frame.bmp");self.runner.capture("host")

    def observer(self,candidate=None):
        created=[];identities=self.identities;paths=self.paths;run=self.run
        class Observer:
            def __init__(self,args):
                self.manifests=paths;self.summary={"runtime_run_id":run,"step":"fixture","passed":False};self.summary_path=HERE/"fixture-summary-not-written.json";self.calls=[];created.append(self)
            def load_manifests(self):pass
            def checked(self,*args):pass
            def persist(self):pass
            def single(self,label,peer,operation,*args):
                self.calls.append(operation)
                if operation=="game_read_tutorial":return {"schema":1,"mainthread_confirmed":True,"lifetime_confirmed":True,"snapshot_uuid":str(uuid.uuid4())}
                if operation=="game_capture_frame":return {"fixture":True}
                if operation=="game_read_native_lobby":return {"fixture_ready":True,"prepare":{"snapshot_id":"fixture-shared"}}
                raise AssertionError("Unexpected game operation "+operation)
        lobby=ModuleType("lobby_ui.native_local_acceptance");lobby.button=Mock(side_effect=AssertionError("No mutation authorized in observer fixture"));lobby.observe=Mock();lobby.ready=lambda value:value["fixture_ready"]
        argv=["native_tutorial_observe.py","--expected-run-id",self.run,"--test-root",str(self.instances)]
        if candidate is not None:argv += ["--candidate-binary",str(candidate)]
        with patch.object(acceptance,"AcceptanceRunner",Observer),patch.object(acceptance,"ROOT",self.workspace),patch.object(sys,"argv",argv),patch.dict(sys.modules,{"lobby_ui.native_local_acceptance":lobby}):
            with self.assertRaises(SystemExit) as exited:runpy.run_path(str(ROOT/"tools/native_tutorial_observe.py"),run_name="__main__")
        return exited.exception.code,created[0]

    def test_observer_plugin_requires_explicit_candidate_before_game_calls(self):
        code,runner=self.observer();self.assertEqual(code,1);self.assertEqual(runner.calls,[]);self.assertIn("Explicit candidate",runner.summary["error"]["cause"])

    def test_observer_hashes_exact_supplied_plugin_for_both_peers(self):
        candidate=self.workspace/"candidate/MT2MultiplayerPlugin.dll";candidate.parent.mkdir();candidate.write_bytes(b"MT2MultiplayerPlugin.dll")
        code,runner=self.observer(candidate);self.assertEqual(code,0);self.assertTrue(runner.summary["passed"])
        self.assertEqual(runner.summary["dll_sha256"],self.identities["host"]["installed_binary_sha256"][candidate.name])
        self.assertEqual({value["module"] for value in runner.summary["candidate_binaries"].values()},{"MT2MultiplayerPlugin.dll"})
        self.assertEqual(len(runner.calls),6)

    def test_observer_legacy_default_artifact_remains_legacy(self):
        self.legacy();candidate=self.workspace/"build/bin/Release/MT2_Multiplayer.dll";candidate.parent.mkdir(parents=True);candidate.write_bytes(b"legacy-fixture")
        code,runner=self.observer();self.assertEqual(code,0);self.assertEqual(runner.summary["candidate_binaries"]["host"]["module"],"MT2_Multiplayer.dll")


if __name__=="__main__":unittest.main()
