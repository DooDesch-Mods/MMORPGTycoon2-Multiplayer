"""Stop only one verified stalled CMake build process tree."""
import psutil
from pathlib import Path
target = psutil.Process(11444)
expected = ['C:\\Program Files\\CMake\\bin\\cmake.exe', '--build', 'F:\\Projects\\Mods\\MMORPGTycoon2\\build', '--config', 'Release', '--parallel', '4']
assert target.cmdline() == expected, 'build command differs'
owner = target.parent()
assert owner.pid == 104856 and owner.cmdline()[-1] == 'tools/build.py', 'build owner differs'
assert Path(owner.cwd()).resolve() == Path(__file__).resolve().parents[1], 'build workspace differs'
children = target.children(recursive=True)
for process in reversed(children):
    try:
        process.kill()
    except psutil.NoSuchProcess:
        print('info: compiler already exited pid=' + str(process.pid))
target.kill()
_, alive = psutil.wait_procs(children + [target], timeout=5)
assert not alive, 'owned build process termination incomplete'
print('info: stalled build tree stopped, root=11444, children=' + str(len(children)))
