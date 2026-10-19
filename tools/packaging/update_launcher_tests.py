from pathlib import Path
p=Path("test/test_local_launcher.py")
s=p.read_text(encoding="utf-8")
old='process.kwargs["creationflags"] == subprocess.CREATE_NO_WINDOW for process in sidecars'
new='process.kwargs["creationflags"] == (subprocess.CREATE_NO_WINDOW | subprocess.CREATE_BREAKAWAY_FROM_JOB) for process in sidecars'
assert s.count(old)==1
p.write_text(s.replace(old,new),encoding="utf-8")
p=Path("tools/packaging/test_steam_test.py")
s=p.read_text(encoding="utf-8-sig")
needle='class SteamTests(unittest.TestCase):\n'
new='''class SteamTests(unittest.TestCase):
    def test_invalid_native_root_fails_before_copy(self):
        import argparse
        args = argparse.Namespace(test_root=Path(tempfile.gettempdir()) / "wrong-name",
                                  source=Path(tempfile.gettempdir()) / "game-source")
        with patch.object(local, "source_files") as copy:
            with self.assertRaisesRegex(local.LaunchError, "basename _MPTest"):
                steam.launch(args, None)
            copy.assert_not_called()

'''
assert s.count(needle)==1
p.write_text(s.replace(needle,new),encoding="utf-8")
