"""Execute the staged cleanup functions with controlled world/thread observations."""
from pathlib import Path
import argparse
import json
import os
import subprocess

HERE = Path(__file__).resolve().parent
OUT = HERE / 'journal-check-current'
FAMILIES = ('npc_quest_list', 'character_rename', 'ability_edit', 'building_status', 'costume_edit', 'dungeon_config', 'dungeon_light', 'dungeon_sound', 'dungeon_palette', 'dungeon_gizmo')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,default=HERE/'candidate')
    parser.add_argument('--emit',type=Path)
    args=parser.parse_args()
    source = '''#include <string>
#include <map>
#include <optional>
#include <cstdint>
#include <iostream>
#include <cstdlib>
namespace a {enum class QuestListEdit{List,Title};}
unsigned actual_thread=17;unsigned GetCurrentThreadId(){return actual_thread;}
namespace native {
struct WorldStatus {int active_mode_index=2;bool title_transition_pending=false;int saves_in_progress=0;};
WorldStatus observed;bool readable=true;
bool read_world_status(WorldStatus& out,std::string& e){if(!readable){e="world read failed";return false;}out=observed;return true;}
}
bool fail(std::string& e,const char* why){e=why;return false;}
namespace quest_history {std::map<std::string,int> journal{{"retained",1}};void clear(){journal.clear();}}
namespace dungeon_cells {bool available=true;unsigned calls=0;bool retire_lobby(std::string& e){++calls;if(!available){e="Dungeon cell retirement failed";return false;}return true;}}
int comparisons=0;
void check(bool ok){++comparisons;if(!ok){std::cerr<<"cleanup assertion "<<comparisons<<" failed\\n";std::exit(1);}}
'''
    for family in FAMILIES:
        text = (args.source/f'src/game/{family}_runtime.inc').read_text(encoding='utf-8')
        start = text.index('bool retire_lobby(std::string& e)')
        end = text.index('bool enable(', start)
        source += f'namespace {family} {{\n'
        source += '''struct State {unsigned thread=17;} state;
bool ready=true,installed=true,fault=true,replaying=false,callback_failed=true;const int* preparation=nullptr;
uintptr_t replay_receiver=77,original=0x1234;uint64_t generation=42,captures=3,submitted=2,rejected=1;
unsigned original_calls=1;std::string cause="previous fault",last_action="old action",ui_token="old token";
bool replay_remove=true;a::QuestListEdit replay_edit=a::QuestListEdit::Title;unsigned replay_property=4;int replay_state=2;int replay_atmosphere=3;bool preparing=true;
struct ViewTail {bool armed=false;};ViewTail view_tail;std::map<std::string,uintptr_t> adjustment_views;
std::optional<int> pending=5;std::map<std::string,int> operations{{"old action",1}},jobs{{"old token",1}};
'''
        source += text[start:end]
        palette = family == 'dungeon_palette'
        checks = '''void run(){
    std::string e;
    const auto retained=[&](){return pending==5&&operations.size()==1&&fault&&cause=="previous fault"&&last_action=="old action"&&replay_receiver==77&&original_calls==1;};
    actual_thread=18;check(!retire_lobby(e)&&e.find("Mainthread")!=std::string::npos);check(retained());actual_thread=17;
    native::readable=false;check(!retire_lobby(e)&&e=="world read failed");check(retained());native::readable=true;
    native::observed.active_mode_index=3;check(!retire_lobby(e)&&e.find("Title")!=std::string::npos);check(retained());native::observed.active_mode_index=2;
    native::observed.title_transition_pending=true;check(!retire_lobby(e));check(retained());native::observed.title_transition_pending=false;
    native::observed.saves_in_progress=1;check(!retire_lobby(e));check(retained());native::observed.saves_in_progress=0;
    replaying=true;check(!retire_lobby(e)&&e.find("interrupt")!=std::string::npos);check(retained());replaying=false;
    check(retire_lobby(e)&&e.empty());check(!pending&&operations.empty()&&!fault&&cause.empty()&&last_action.empty()&&replay_receiver==0&&original_calls==0);
    check(ready&&installed&&original==0x1234&&generation==42&&captures==3&&submitted==2&&rejected==1);
'''
        if palette:
            checks = checks.replace('&&replay_receiver==0&&original_calls==0', '')
            checks = checks.replace('    actual_thread=18;', '    dungeon_cells::available=false;check(!retire_lobby(e)&&e=="Dungeon cell retirement failed");check(retained());dungeon_cells::available=true;\n    actual_thread=18;')
        source += checks
        if family == 'npc_quest_list':
            source += '    check(ui_token.empty()&&jobs.empty()&&!replay_remove&&replay_edit==a::QuestListEdit::List&&quest_history::journal.empty());\n'
        elif family == 'character_rename':
            source += '    check(replay_property==0);\n'
        elif family == 'dungeon_gizmo':
            source += '    check(preparation==nullptr&&!callback_failed);\n'
        elif family == 'dungeon_sound':
            source += '    check(replay_atmosphere==-1&&!preparing);\n'
        elif family == 'building_status':
            source += '    check(replay_state==-1);\n'
        source += '''    check(retire_lobby(e)&&e.empty()&&ready&&installed&&generation==42);
    ready=false;fault=true;cause="installation failed";native::readable=false;
    check(retire_lobby(e)&&fault&&cause=="installation failed"&&installed&&generation==42);native::readable=true;
}}
'''
    source += 'int main(){'+''.join(f'{family}::run();' for family in FAMILIES)+'std::cout<<"PASS "<<comparisons<<" actual cleanup comparisons; no native calls\\n";}\n'
    cpp = args.emit or OUT / 'test_journal_retirement.cpp'
    cpp.parent.mkdir(parents=True,exist_ok=True)
    cpp.write_text(source, encoding='utf-8')
    if args.emit:
        print('Generated exact current Character-family cleanup-body checks')
        return 0
    exe = OUT / 'test_journal_retirement.exe'
    env = os.environ.copy();env.update(TEMP=str(HERE/'compiler-temp'), TMP=str(HERE/'compiler-temp'))
    compile_command = ['C:/Program Files/LLVM/bin/clang++.exe', '-std=c++20', '-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH=1', str(cpp), '-o', str(exe)]
    build = subprocess.run(compile_command, env=env, capture_output=True, text=True, encoding='utf-8')
    (OUT/'test-compile.log').write_text(build.stdout+build.stderr, encoding='utf-8')
    if build.returncode:
        print('FAIL journal cleanup compile');return build.returncode
    result = subprocess.run([str(exe)], capture_output=True, text=True, encoding='utf-8')
    (OUT/'test-run.log').write_text(result.stdout+result.stderr, encoding='utf-8')
    (OUT/'test-result.json').write_text(json.dumps({'status': 'PASS' if result.returncode==0 else 'FAIL', 'native_calls': 0, 'output': result.stdout, 'error': result.stderr}, indent=2)+'\n', encoding='utf-8')
    print(result.stdout or result.stderr)
    return result.returncode

if __name__ == '__main__':
    raise SystemExit(main())
