"""Extract the production Economy observation block and check diagnostic routing."""
import argparse
from pathlib import Path

def extract(root, output):
    source=(root/'src/game/typed_action_runtime.cpp').read_text(encoding='utf-8-sig')
    header=(root/'src/game/typed_action_runtime.h').read_text(encoding='utf-8-sig')
    core=(root/'src/core/dll_main.cpp').read_text(encoding='utf-8-sig')
    assert 'json status() {return status(true);}' in source
    assert 'nlohmann::json status();' in header
    assert 'nlohmann::json status(bool inspect_native_economy);' in header
    assert 'static json status(bool inspect_native_economy=true);' in core
    assert core.count('game::typed_action_runtime::status(inspect_native_economy);')==1
    assert core.count('write_json(dir/"status.json",status(false)); last_status=now;')==1
    # The ordered native Save path also reads the verified write root without
    # triggering a diagnostic economy rescan.
    assert core.count('const auto actual_root=status(false).at("write_dir").get<std::string>();')==1
    assert core.count('status(false)')==2
    assert '{"native_economy_source",economy_source},{"native_economy_revision",economy_revision}' in source
    start=source.index('json status(bool inspect_native_economy) {')
    begin=source.index('    nlohmann::json native_economy=',start)
    end=source.index('    return {{"initialized",true}',begin)
    # Wrong-thread and absent-runtime rejection remains before any observation.
    prefix=source[start:begin]
    assert 'if(!runtime)return {{"initialized",false}};' in prefix
    assert 'runtime->thread!=GetCurrentThreadId()' in prefix
    assert 'const auto s=runtime->controller.status();' in prefix
    output.mkdir(parents=True,exist_ok=True)
    (output/'economy_status_body.inc').write_text(source[begin:end],encoding='utf-8',newline='\n')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();extract(args.source_root,args.output)
