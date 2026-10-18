"""Reuse the existing owned-memory fixtures and check exact native effect order."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
life=(a.source_root/'test/test_vip_alert_lifecycle.cpp').read_text(encoding='utf-8')
(a.output/'disconnect_type8_fixture.inc').write_text(life[life.index('struct Fake:'):life.index('#ifdef _WIN32\nnamespace hook=')],encoding='utf-8')
reader=(a.source_root/'test/test_vip_alert_client_reader.cpp').read_text(encoding='utf-8')
(a.output/'disconnect_memory_fixture.inc').write_text(reader[reader.index('constexpr uintptr_t image='):reader.index('\nunsigned checks=0;')],encoding='utf-8')
host_fixture=(a.source_root/'test/test_vip_host_witness.cpp').read_text(encoding='utf-8')
assert host_fixture.count('int main(){try{')==1
(a.output/'disconnect_host_fixture.inc').write_text(host_fixture.replace('int main(){try{','int retained_vip_host_main(){try{'),encoding='utf-8')
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
host=(a.source_root/'src/game/scenario_vip_authority/host_witness.cpp').read_text(encoding='utf-8')
update=body(host,'void host_update(')
assert update.index('observe(true,std::nullopt)')<update.index('try{original(receiver,dt)')<update.index('after_end(producer,receiver,before,message,e)')<update.index('observe(false,message)')
assert 'same_world' in host and 'return pin(p,e);' in body(host,'bool after_end(')
native=(a.source_root/'src/game/scenario_vip_authority/alert_client_native.cpp').read_text(encoding='utf-8')
assert 'o.image+0x2330a0' in body(native,'bool NativePort::create_message(')
assert 'o.image+0x2349a0' in body(native,'bool NativePort::fire_message(')
assert 'disconnect_read::inspect(memory,o.image,graph,alert,0,nullptr' in body(native,'bool NativePort::discard_message(')
invoke=(a.source_root/'src/game/scenario_vip_authority/disconnect_invoke.h').read_text(encoding='utf-8')
assert invoke.index('f.construct(storage,text)')<invoke.index('f.factory(type,storage)')<invoke.index('f.arguments(storage+0x20)')<invoke.index('f.string(storage)')
assert 'int32_t type=9' in invoke
assert 'alignas(8) unsigned char storage[0x38]' in invoke and '__finally' in invoke
print('PASS exact original-Update, type9 factory/FireAndForget and native LocString lifetime boundaries')
