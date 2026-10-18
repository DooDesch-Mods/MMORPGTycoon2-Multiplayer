from pathlib import Path
import argparse,re
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
core=(a.root/'src/core/dll_main.cpp').read_text(encoding='utf-8')
native=(a.root/'src/game/native_engine.cpp').read_text(encoding='utf-8')
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
start=core.index('using EnginePhaseUpdate=')
end=core.index('static DWORD render_thread=',start)
block=core[start:end]
assert block.count('apply_timing::Scope measured(')==3
assert block.count('hooks::create_hook(')==3
assert 'using EnginePhaseDraw=void(*)(void*,void*,void*);' in block
assert 'original_timed_screen_draw_pipeline(receiver,pipeline,options);' in block
assert 'ScreenDraw)' not in block
assert 'MH_' not in block and 'PageReadScope' not in block
(a.output/'engine_phase_hooks.inc').write_text(block)
gate='        if(owned_native_profile&&Config::instance().local_test&&!install_engine_timing_hooks(native_error))\n            throw std::runtime_error(native_error);\n'
assert core.count(gate)==1
initialize=core.index('bool initialize(HMODULE module)')
assert core.index('if(!game::native::initialize(native_error))',initialize)<core.index(gate)<core.index('if(!hooks::enable_all())',initialize)
assert core.count('install_engine_timing_hooks(')==2
entry=body(native,'void* entrypoint(')
(a.output/'engine_phase_entry.inc').write_text(entry)
guards=[]
for rva in ['4a1bb0','9ca0e0','9cb150']:
    matches=re.findall(r'        \{0x'+rva+r', "([^"\n]+)", (\d+)\},',native)
    assert len(matches)==1
    data,count=matches[0];assert len(re.findall(r'\\x[0-9a-f]{2}',data))==int(count)
    guards.append('{0x'+rva+', "'+data+'", '+count+'}')
(a.output/'engine_phase_guards.inc').write_text(',\n'.join(guards))
print('Three exact wrappers, local diagnostic gate and resolver guards extracted')
