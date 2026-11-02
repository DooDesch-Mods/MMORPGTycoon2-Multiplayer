"""Extract the actual ordered proof parent and its bounded native reader tree."""
from pathlib import Path
import argparse
import hashlib

parser=argparse.ArgumentParser()
parser.add_argument('--root',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
def source(path):return (args.root/path).read_text(encoding='utf-8')
def body(text,anchor):
    start=text.index(anchor);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
def emit(name,pieces):
    (args.output/name).write_text('\n'.join(pieces)+'\n',encoding='utf-8')

witness=source('src/game/native_open_witness.cpp')
attributes=body(witness,'DWORD WINAPI timed_account_attributes(')
assert attributes.count('attributes=GetFileAttributesW(path);')==1
attributes=attributes.replace('attributes=GetFileAttributesW(path);','attributes=observed_account_attributes(path);',1)
emit('read_origin_binding.inc',[attributes,body(witness,'bool framework_binding(')])
helpers=[body(witness,key) for key in ['bool fail(', 'bool readable(', 'DWORD copy(', 'template<class T> bool read(', 'bool vt(', 'bool mode_singleton(', 'bool world_fields(', 'bool world_witness(']]
helpers[2]=helpers[2].replace('DWORD copy(','DWORD native_copy(',1)
emit('read_origin_helpers.inc',helpers)
start=witness.index('ReadOriginTransaction::ReadOriginTransaction()')
end=witness.index('\nStatus status()',start)
emit('read_origin_native.inc',[witness[start:end],body(witness,'Status status()')])
runtime=source('src/game/native_open_runtime.cpp')
emit('read_origin_runtime.inc',[body(runtime,'bool read_current_world(witness::CurrentWorld& result,std::string& error) {'),body(runtime,'bool read_current_world(witness::CurrentWorld& result,std::string& error,witness::ReadOriginTransaction& transaction) {')])
snapshot=source('src/net/ordered_snapshot_preload.inc')
load=source('src/net/ordered_snapshot_load.inc')
pieces=[body(snapshot,key) for key in ['static bool ordered_owner(', 'static bool ordered_fresh(', 'static bool ordered_native_read(', 'static bool read_ordered_snapshot_impl(', 'static bool read_ordered_snapshot_local_impl(']]
pieces[2:2]=[body(load,key) for key in ['static bool ordered_read_normal(', 'static bool ordered_loaded_observation(']]
for name in ['read_ordered_snapshot','read_ordered_snapshot_local']:
    pieces.append(body(snapshot,f'bool {name}(OrderedSnapshotIdentity& out,std::string& error){{'))
    pieces.append(body(snapshot,f'bool {name}(OrderedSnapshotIdentity& out,std::string& error,game::building_mixed_identity::opened_file::ReadOriginTransaction& transaction){{'))
emit('read_origin_snapshot.inc',pieces)
parent=body(source('src/game/ordered_session_proof.h'),'template<bool RecoveryLocal=false>')
baseline=parent.replace('    building_mixed_identity::opened_file::ReadOriginTransaction transaction;\n','',1)
baseline=baseline.replace('identity,error,transaction)','identity,error)').replace('current,error,transaction)','current,error)')
baseline=baseline.replace('    if(!transaction.finish(error))return false;\n','',1)
# The only parent change is explicit transaction lifetime and reader routing.
expected='b56908989ff63411a0aecf51590382ed128c265cfa34fec53bba206147c51c20'
assert hashlib.sha256(baseline.encode()).hexdigest()==expected,'Ordered proof parent changed outside explicit transaction routing'
emit('read_origin_parent.inc',[baseline.replace('read_native_proof_impl(', 'baseline_read_native_proof_impl(',1),parent])
print('info: extracted actual ordered parent, snapshot, runtime and complete native readers')
