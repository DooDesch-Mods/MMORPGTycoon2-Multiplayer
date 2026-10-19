from pathlib import Path
p=Path("tools/steam_test.py")
s=p.read_text(encoding="utf-8-sig").replace("+    # Avoid", "    # Avoid")
p.write_text(s,encoding="utf-8")
p=Path("tools/local_test.py")
s=p.read_text(encoding="utf-8")
old='def source_files(source, build):'
assert s.count(old)==1
s=s.replace(old,'def source_files(source, build, *, original_steam=None):')
old='''    for name in ("steam_api64_original.dll",):
        if not (source / name).is_file():
            raise LaunchError(f"original game dependency missing: {source / name}")'''
new='''    original_steam = original_steam or source / "steam_api64_original.dll"
    if not original_steam.is_file():
        raise LaunchError(f"original game dependency missing: {original_steam}")
    files["steam_api64_original.dll"] = original_steam'''
assert s.count(old)==1
p.write_text(s.replace(old,new),encoding="utf-8")
