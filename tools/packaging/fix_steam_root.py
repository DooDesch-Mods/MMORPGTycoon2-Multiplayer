from pathlib import Path
p=Path("tools/steam_test.py")
s=p.read_text(encoding="utf-8")
s=s.replace('    source = args.source.resolve()\n', '    source = args.source.resolve()\n    if root.name != "_MPTest":\n        raise local.LaunchError("native snapshot profile requires test-root basename _MPTest")\n')
s=s.replace('default=local.ROOT / "_SteamTest"', 'default=local.ROOT / "_SteamTest" / "_MPTest"')
s=s.replace('raise local.LaunchError(f"Steam bootstrap timed out required={args.ready_timeout}s: {reason}")', 'raise local.LaunchError(f"Steam bootstrap timed out required={args.ready_timeout}s: {reason}; native log:\\n{local.log_tail(game / chr(109)+chr(116))}")') if False else s
old='raise local.LaunchError(f"Steam bootstrap timed out required={args.ready_timeout}s: {reason}")'
new='raise local.LaunchError(f"Steam bootstrap timed out required={args.ready_timeout}s: {reason}; native log:\\n{local.log_tail(game / \'mt2mp.log\')}")'
assert s.count(old)==1
s=s.replace(old,new)
p.write_text(s,encoding="utf-8")
