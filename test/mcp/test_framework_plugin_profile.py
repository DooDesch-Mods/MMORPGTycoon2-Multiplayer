"""Strict schema-2 Framework plugin admission using owned filesystem fixtures."""
from pathlib import Path
import copy
import hashlib
import json
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch
import uuid

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/"tools/mcp"))
from runtime import (Runtime,RuntimeErrorDetail,atomic_json,read_json,FRAMEWORK_MODULE_NAMES,
                     candidate_binary_receipt,framework_module_paths,manifest_runtime_kind,runtime_diagnostic_directory)


class FrameworkPluginTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=HERE);self.addCleanup(self.temp.cleanup)
        self.instances=Path(self.temp.name)/"_MPTest";self.root=self.instances/"host";self.root.mkdir(parents=True)
        self.profile=self.root/"profile";self.mailbox=self.profile/"diagnostics/multiplayer";self.mailbox.mkdir(parents=True)
        self.framework_mailbox=self.profile/"diagnostics"
        self.manifest={"schema_version":2,"runtime_kind":"framework-plugin","framework_profile":str(self.profile),
                       "run_id":str(uuid.uuid4()),"pid":222,"process_start_time":"133400000000000000",
                       "runtime_root":str(self.root),"executable_path":str(self.root/"game/MT2.exe"),"peer_id":1}
        self.manifest["owner"]={"kind":"MPTest","run_id":self.manifest["run_id"]}
        self.modules=framework_module_paths(self.manifest)
        for name,path in self.modules.items():path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes((name+"-fixture").encode())
        self.manifest["installed_binary_sha256"]={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in self.modules.items()}
        self.path=self.root/"instance.json";self.save()
        self.process={key:self.manifest[key] for key in ("pid","process_start_time","executable_path")};self.process["owned_by_current_user"]=True
        self.processes={222:self.process}
        self.loaded=[str(path) for path in self.modules.values()]
        self.runtime=Runtime(self.instances,inspector=lambda pid:self.processes.get(pid),module_inspector=lambda pid:self.loaded,timeout=1)
        self.status()

    def save(self,value=None):atomic_json(self.path,self.manifest if value is None else value)
    def status(self):
        atomic_json(self.mailbox/"status.json",{"schema_version":1,"run_id":self.manifest["run_id"],"pid":self.manifest["pid"],
                    "runtime_kind":"framework-plugin","framework_profile":self.manifest["framework_profile"],
                    "installed_binary_sha256":self.manifest["installed_binary_sha256"],"game_thread_ready":True,"capabilities":{"status":True,"inspect_engine":True}})
    def bind(self):return self.runtime.bind(str(self.path))
    def error(self,code,operation):
        with self.assertRaises(RuntimeErrorDetail) as caught:operation()
        self.assertEqual(caught.exception.code,code,str(caught.exception));return caught.exception
    def lock(self,**changes):
        value={"schema_version":1,"id":str(uuid.uuid4()),"run_id":self.manifest["run_id"],"pid":self.manifest["pid"],
               "process_start_time":self.manifest["process_start_time"],"runtime_root":self.manifest["runtime_root"],
               "executable_path":self.manifest["executable_path"],"owner":self.manifest["owner"],
               "runtime_kind":"framework-plugin","framework_profile":str(self.profile),"op":"inspect_engine"}
        value.update(changes);atomic_json(self.mailbox/"request.lock",value);return value

    def test_positive_exact_five_modules_and_separate_status(self):
        result=self.bind();self.assertTrue(result["process_identity_verified"])
        self.assertEqual(self.runtime._diagnostics(),self.mailbox)
        self.assertEqual(self.runtime.available(),{"status","inspect_engine"})
        self.assertEqual(set(framework_module_paths(self.manifest)),set(FRAMEWORK_MODULE_NAMES))

    def test_stock_steam_filename_required_without_renaming(self):
        value=copy.deepcopy(self.manifest);value["installed_binary_sha256"]["originalsteam_api64.dll"]=value["installed_binary_sha256"].pop("steam_api64.dll")
        self.save(value);self.error("invalid_manifest",self.bind)

    def test_unknown_kind_schema_and_downgrade_rejected(self):
        for changes in ({"runtime_kind":"standalone"},{"runtime_kind":"framework"},{"schema_version":3},{"schema_version":True},{"schema_version":1}):
            with self.subTest(changes=changes):self.save({**self.manifest,**changes});self.error("invalid_manifest",self.bind)

    def test_missing_or_extra_module_hash_names_rejected(self):
        for name in FRAMEWORK_MODULE_NAMES:
            value=copy.deepcopy(self.manifest);value["installed_binary_sha256"].pop(name)
            with self.subTest(missing=name):self.save(value);self.error("invalid_manifest",self.bind)
        value=copy.deepcopy(self.manifest);value["installed_binary_sha256"]["MT2_Multiplayer.dll"]="a"*64
        self.save(value);self.error("invalid_manifest",self.bind)

    def test_each_file_hash_is_verified(self):
        for name,path in self.modules.items():
            original=path.read_bytes();path.write_bytes(b"changed")
            with self.subTest(name=name):self.error("module_changed",self.bind)
            path.write_bytes(original)

    def test_each_exact_loaded_path_required(self):
        for name,path in self.modules.items():
            self.loaded=[str(item) for item in self.modules.values() if item!=path]
            with self.subTest(name=name):self.error("module_identity_failed",self.bind)
        self.loaded=[str(item) for item in self.modules.values() if item.name!="MT2MultiplayerPlugin.dll"]+[str(self.root/"game/MT2MultiplayerPlugin.dll")]
        self.error("module_identity_failed",self.bind)

    def test_legacy_dll_cannot_substitute_plugin(self):
        self.loaded=[path for path in self.loaded if not path.endswith("MT2MultiplayerPlugin.dll")]+[str(self.root/"game/MT2_Multiplayer.dll")]
        self.error("module_identity_failed",self.bind)

    def test_wrong_profile_root_executable_and_reparse_alias_rejected(self):
        for field,path in (("framework_profile",self.root/"other-profile"),("framework_profile",self.root.parent/"profile"),("executable_path",self.root/"MT2.exe")):
            with self.subTest(field=field,path=path):self.save({**self.manifest,field:str(path)});self.error("path_escape",self.bind)
        self.save()
        with patch.object(Path,"is_junction",lambda path:path==self.profile):self.error("path_escape",self.bind)

    def test_process_start_path_and_owner_remain_strict(self):
        original=copy.deepcopy(self.process)
        for field,value,code in (("process_start_time","999","pid_reused"),("executable_path",str(self.root/"other.exe"),"ownership_failed"),("owned_by_current_user",False,"ownership_failed")):
            self.process[field]=value
            with self.subTest(field=field):self.error(code,self.bind)
            self.process.clear();self.process.update(original)
        value=copy.deepcopy(self.manifest);value["owner"]["kind"]="Framework";self.save(value);self.error("ownership_failed",self.bind)

    def test_status_mode_profile_and_all_hashes_must_match(self):
        self.bind()
        for field,value in (("runtime_kind","standalone"),("framework_profile",str(self.root/"other")),("installed_binary_sha256",{})):
            self.status();status=read_json(self.mailbox/"status.json");status[field]=value;atomic_json(self.mailbox/"status.json",status)
            with self.subTest(field=field):self.error("stale_status",self.runtime.available)

    def test_framework_own_status_never_supplies_plugin_capability(self):
        self.bind();(self.mailbox/"status.json").unlink()
        atomic_json(self.framework_mailbox/"status.json",{"schema_version":1,"run_id":self.manifest["run_id"],"pid":222,"game_thread_ready":True,"capabilities":{"status":True}})
        self.error("read_failed",self.runtime.available)

    def test_plugin_request_ack_keeps_protocol_and_framework_mailbox_untouched(self):
        self.bind();foreign=self.framework_mailbox/"request.json";foreign.write_text('{"framework":"owned"}')
        failures=[]
        def respond():
            try:
                path=self.mailbox/"request.json";deadline=time.monotonic()+2
                while not path.exists():
                    if time.monotonic()>deadline:raise AssertionError("request not published")
                    time.sleep(.001)
                request=read_json(path);atomic_json(self.mailbox/"response.json",{"schema_version":1,"id":request["id"],"run_id":request["run_id"],"ok":True,"result":{"native":True}})
            except Exception as error:failures.append(error)
        thread=threading.Thread(target=respond);thread.start()
        try:self.assertTrue(self.runtime.request("inspect_engine",{})["result"]["native"])
        finally:thread.join(3)
        self.assertFalse(thread.is_alive());self.assertEqual(failures,[])
        self.assertEqual(foreign.read_text(),'{"framework":"owned"}')
        self.assertEqual(read_json(self.mailbox/"request.json")["schema_version"],1)

    def test_timeout_retains_mode_owned_lock_and_no_replay(self):
        self.bind();self.runtime.timeout=.05;self.error("observation_timeout",lambda:self.runtime.request("status",{}))
        first=read_json(self.mailbox/"request.json");record=read_json(self.mailbox/"request.lock")
        self.assertEqual(record["runtime_kind"],"framework-plugin");self.assertEqual(record["framework_profile"],str(self.profile))
        self.assertTrue(self.runtime.observe_pending()["pending"])
        self.error("mailbox_busy",lambda:self.runtime.request("status",{}));self.assertEqual(read_json(self.mailbox/"request.json"),first)

    def test_wrong_mode_profile_or_owner_lock_never_released(self):
        self.bind()
        for changes in ({"runtime_kind":"standalone"},{"framework_profile":str(self.root/"other")},{"owner":{"kind":"Framework"}},{"runtime_kind":None}):
            self.lock(**changes)
            with self.subTest(changes=changes):self.error("foreign_lock",self.runtime.observe_pending)
            self.assertTrue((self.mailbox/"request.lock").exists())

    def test_legacy_pending_lock_blocks_framework_bind(self):
        legacy=self.root/"diagnostics";legacy.mkdir();atomic_json(legacy/"request.lock",{"id":str(uuid.uuid4())})
        self.error("pending_request",self.bind);self.assertTrue((legacy/"request.lock").exists())

    def test_previous_framework_lock_requires_exact_process_exit(self):
        self.bind();record=self.lock()
        old=copy.deepcopy(self.manifest);self.manifest["pid"]=333;self.manifest["run_id"]=str(uuid.uuid4());self.manifest["owner"]={"kind":"MPTest","run_id":self.manifest["run_id"]};self.save()
        self.processes[333]={**self.process,"pid":333}
        other=Runtime(self.instances,inspector=lambda pid:self.processes.get(pid),module_inspector=lambda pid:self.loaded,timeout=.05);other.bind(str(self.path))
        self.error("prior_process_alive",other.observe_pending)
        self.processes[222]=None;result=other.observe_pending()
        self.assertEqual(result["previous_run_id"],old["run_id"]);self.assertFalse((self.mailbox/"request.lock").exists())

    def test_explicit_old_framework_receipt_recovers_after_exit_even_if_build_changed(self):
        self.bind();record=self.lock();previous=self.root/"archive/instance.json";previous.parent.mkdir();atomic_json(previous,self.manifest)
        self.processes[222]=None;self.modules["MT2MultiplayerPlugin.dll"].write_bytes(b"new-build")
        unbound=Runtime(self.instances,inspector=lambda pid:self.processes.get(pid),module_inspector=lambda pid:self.loaded)
        result=unbound.recover_pending(str(previous));self.assertTrue(result["process_exit_verified"]);self.assertFalse((self.mailbox/"request.lock").exists())

    def test_explicit_legacy_receipt_clears_only_legacy_mailbox(self):
        legacy={key:self.manifest[key] for key in ("run_id","pid","process_start_time","runtime_root","executable_path","owner")};legacy["schema_version"]=1
        previous=self.root/"archive/instance.json";previous.parent.mkdir();atomic_json(previous,legacy)
        legacy_dir=self.root/"diagnostics";legacy_dir.mkdir();record={"id":str(uuid.uuid4()),"run_id":legacy["run_id"],"pid":222,"process_start_time":legacy["process_start_time"]};atomic_json(legacy_dir/"request.lock",record)
        untouched=self.lock();self.processes[222]=None
        unbound=Runtime(self.instances,inspector=lambda pid:self.processes.get(pid))
        self.assertTrue(unbound.recover_pending(str(previous))["process_exit_verified"])
        self.assertEqual(read_json(self.mailbox/"request.lock"),untouched)

    def test_candidate_helper_hashes_selected_plugin_and_never_guesses(self):
        plugin=self.modules["MT2MultiplayerPlugin.dll"]
        result=candidate_binary_receipt(self.manifest,plugin);self.assertEqual(result["module"],plugin.name)
        self.error("candidate_required",lambda:candidate_binary_receipt(self.manifest,None))
        wrong=self.root/"game/MT2_Multiplayer.dll";wrong.write_bytes(plugin.read_bytes())
        self.error("candidate_mismatch",lambda:candidate_binary_receipt(self.manifest,wrong))
        plugin.write_bytes(b"changed");self.error("candidate_mismatch",lambda:candidate_binary_receipt(self.manifest,plugin))

    def test_legacy_helper_keeps_selected_legacy_filename(self):
        path=self.root/"game/MT2_Multiplayer.dll";path.write_bytes(b"legacy")
        value={"schema_version":1,"runtime_root":str(self.root),"installed_binary_sha256":{path.name:hashlib.sha256(path.read_bytes()).hexdigest()}}
        self.assertEqual(manifest_runtime_kind(value),"standalone")
        self.assertEqual(runtime_diagnostic_directory(value),self.root/"diagnostics")
        self.assertEqual(candidate_binary_receipt(value,path)["module"],"MT2_Multiplayer.dll")


if __name__=="__main__":unittest.main()
