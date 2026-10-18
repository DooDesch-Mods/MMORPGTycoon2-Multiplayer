"""Exact module admission for the existing read-only lifecycle witness."""
from pathlib import Path
import copy
import hashlib
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
sys.path.insert(0,str(ROOT/"tools/mcp"))
from runtime import RuntimeErrorDetail,framework_module_paths
import subscriber_lifecycle_read as reader


class FrameworkReaderTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(dir=HERE);self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)/"_MPTest/host"
        self.manifest={"schema_version":2,"runtime_kind":"framework-plugin","runtime_root":str(self.root),
                       "framework_profile":str(self.root/"profile"),"executable_path":str(self.root/"game/MT2.exe")}
        self.paths=framework_module_paths(self.manifest);self.rows=[];hashes={}
        for index,(name,path) in enumerate(self.paths.items()):
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(name.encode());hashes[name]=hashlib.sha256(path.read_bytes()).hexdigest()
            self.rows.append({"path":str(path),"base":0x140000000+index*0x10000000,"size":0x1000000})
        self.manifest["installed_binary_sha256"]=hashes
        self.pin=patch.object(reader,"INSTALLED_EXE_SHA256",hashes["MT2.exe"]);self.pin.start();self.addCleanup(self.pin.stop)

    def test_framework_reader_requires_five_fixed_paths(self):
        self.assertEqual(set(reader.modules_guard(self.manifest,self.rows)),set(self.paths))

    def test_legacy_or_wrong_path_cannot_substitute_selected_plugin(self):
        for path in (self.root/"game/MT2_Multiplayer.dll",self.root/"game/MT2MultiplayerPlugin.dll"):
            rows=copy.deepcopy(self.rows);rows[-1]["path"]=str(path)
            with self.subTest(path=path),self.assertRaisesRegex(RuntimeErrorDetail,"missing/duplicated"):reader.modules_guard(self.manifest,rows)

    def test_hash_and_unique_loaded_row_remain_required_for_each_module(self):
        for name,path in self.paths.items():
            original=path.read_bytes();path.write_bytes(b"changed")
            with self.subTest(hash=name),self.assertRaisesRegex(RuntimeErrorDetail,"hash changed"):reader.modules_guard(self.manifest,self.rows)
            path.write_bytes(original)
        for row in self.rows:
            with self.subTest(duplicate=row["path"]),self.assertRaisesRegex(RuntimeErrorDetail,"missing/duplicated"):reader.modules_guard(self.manifest,self.rows+[row])

    def test_unknown_schema2_kind_is_not_a_legacy_reader(self):
        self.manifest["runtime_kind"]="standalone"
        with self.assertRaisesRegex(RuntimeErrorDetail,"Unknown manifest"):reader.modules_guard(self.manifest,self.rows)


if __name__=="__main__":unittest.main()
