from pathlib import Path
import hashlib,json,sys
root=Path(sys.argv[1]);out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True)
text=(root/'src/core/dll_main.cpp').read_text(encoding='utf-8-sig')
begin='        else if(operation=="load_snapshot") {';end='        else { response["ok"]=false; response["error"]={{"code","CAPABILITY_UNAVAILABLE"}'
assert text.count(begin)==1 and text.count(end)==1
body=text[text.index(begin):text.index(end)].replace('else if(operation=="load_snapshot")','if(operation=="load_snapshot")',1)
(out/'selected_snapshot_load_actual.inc').write_text(body,encoding='utf-8')
(out/'body.json').write_text(json.dumps({'source':'src/core/dll_main.cpp','sha256':hashlib.sha256(body.encode()).hexdigest()},indent=2)+'\n')
text=(root/'src/game/lobby_operations.cpp').read_text(encoding='utf-8-sig')
body=text[text.index('namespace mt2mp::game::lobby_operations {'):]
(out/'selected_snapshot_lobby_actual.inc').write_text(body,encoding='utf-8')
(out/'lobby-body.json').write_text(json.dumps({'source':'src/game/lobby_operations.cpp','sha256':hashlib.sha256(body.encode()).hexdigest()},indent=2)+'\n')
text=(root/'src/game/native_engine.cpp').read_text(encoding='utf-8-sig')
begin='bool request_return_to_title(std::string& error) {'
end='bool request_exit_game(std::string& error) {'
assert text.count(begin)==1 and text.count(end)==1
body=text[text.index(begin):text.index(end)]
assert body.count('retire_autosave_companion(error)')==1
assert body.index('retire_autosave_companion(error)')<body.index('call_string(0x4a2f90, mode, "Quit")')
(out/'selected_snapshot_quit_actual.inc').write_text(body,encoding='utf-8')
(out/'quit-body.json').write_text(json.dumps({'source':'src/game/native_engine.cpp','sha256':hashlib.sha256(body.encode()).hexdigest()},indent=2)+'\n')
