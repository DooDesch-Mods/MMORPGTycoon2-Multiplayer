"""Merge the sealed Move adapter while preserving later Root guards."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
stage=ROOT/'tools/move_integration'
freeze=json.loads((stage/'frozen.json').read_text())
for name,digest in freeze['source_sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest()==digest,name
for name in ('ui_move_runtime.h','ui_move_runtime.cpp','move_contract.h','move_contract.cpp'):
    target=ROOT/'src/game'/name
    assert not target.exists(),str(target)
    target.write_bytes((stage/name).read_bytes())
folder=ROOT/'src/game/move_kiss';folder.mkdir(exist_ok=False)
for name in ('move_kiss.h','move_kiss.cpp','native_calls.cpp'):
    data=(ROOT/'tools/move_kiss'/name).read_text()
    if name=='move_kiss.h':data=data.replace('#include "action_session.h"','#include "../typed_action/action_session.h"')
    (folder/name).write_text(data,encoding='utf-8')
for name in ('typed_action_runtime.cpp','typed_action_runtime.h'):
    target=ROOT/'src/game'/name;content=target.read_text()
    lines=(stage/(name+'.patch')).read_text().splitlines(keepends=True)
    index=0
    while index<len(lines):
        if not lines[index].startswith('@@ '):index+=1;continue
        index+=1;before=[];after=[]
        while index<len(lines) and not lines[index].startswith('@@ '):
            line=lines[index];index+=1
            if line.startswith((' ','-')):before.append(line[1:])
            if line.startswith((' ','+')):after.append(line[1:])
        old,new=''.join(before),''.join(after)
        assert content.count(old)==1,'Move merge context differs: '+name
        content=content.replace(old,new,1)
    if name.endswith('.cpp'):
        assert 'shared removal native object differs from Host before state' in content,'later Root guard lost'
    target.write_text(content,encoding='utf-8')
print('info: sealed Move sources and narrow bridge hunks integrated')
