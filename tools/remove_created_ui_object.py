"""Remove the exact confirmed UI-created test object through the game MCP."""
import argparse
import subprocess
import sys
import uuid
from pathlib import Path
from local_acceptance import ROOT, read_json

def exact_placement(placed,frozen):
    if placed.get('passed') is not True or placed.get('state')!='confirmed-normal-building-UI-two':
        raise RuntimeError('normal UI placement not confirmed; no Remove attempted')
    for field in ('runtime_run_id','action_uuid'):
        value=placed.get(field)
        try: valid=isinstance(value,str) and str(uuid.UUID(value))==value
        except ValueError: valid=False
        if not valid: raise RuntimeError('placement canonical '+field+' missing; no Remove attempted')
    checks=placed.get('checks',[])
    def one(name):
        matches=[check.get('result') for check in checks if check.get('check')==name]
        if len(matches)!=1 or not isinstance(matches[0],dict): raise RuntimeError('placement exact check missing or ambiguous: '+name)
        return matches[0]
    identities=one('instance-manifest-identity')
    result=one('normal-UI-building-confirmed');receipt=result.get('receipt',{})
    if receipt.get('action_uuid')!=placed['action_uuid'] or receipt.get('confirmed') is not True:
        raise RuntimeError('placement confirmed action UUID differs; no Remove attempted')
    for field,required in (('capture_count',1),('host_confirmed_count',1),('exception',0)):
        if type(receipt.get(field)) is not int or receipt[field]!=required: raise RuntimeError('placement actual capture receipt differs: '+field)
    key='action:'+placed['action_uuid'];objects=result.get('objects',{})
    for peer in ('host','client-1'):
        identity=identities.get(peer,{})
        if identity.get('run_id')!=placed['runtime_run_id']: raise RuntimeError('placement actual owned run differs: '+peer)
        if not frozen.get('dll_sha256') or identity.get('installed_binary_sha256',{}).get('MT2_Multiplayer.dll')!=frozen['dll_sha256']:
            raise RuntimeError('placement actual DLL proof differs: '+peer)
        row=objects.get(peer,{})
        if row.get('record_key')!=key or type(row.get('local_uid')) is not int or row['local_uid']<=0:
            raise RuntimeError('placement actual created object key/UID binding differs: '+peer)
        if row.get('revoked') is not False or row.get('unknown') is not False: raise RuntimeError('placement created binding revoked or Unknown: '+peer)
    for field in ('snapshot_id','type','variant','x','y','z'):
        if not objects['host'].get(field) or objects['host'].get(field)!=objects['client-1'].get(field):
            if field in ('x','y','z') and objects['host'].get(field)==0 and objects['client-1'].get(field)==0: continue
            raise RuntimeError('placement actual shared object binding differs: '+field)
    return objects['host']

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--placement', type=Path, required=True)
    parser.add_argument('--freeze', type=Path, required=True)
    args = parser.parse_args()
    placed = read_json(args.placement)
    frozen = read_json(args.freeze)
    building=exact_placement(placed,frozen)
    command = [sys.executable, '-X', 'utf8', str(ROOT / 'tools/ui_remove_acceptance.py'), '--expected-run-id', placed['runtime_run_id'], '--expected-dll-sha256', frozen['dll_sha256'], '--target-key', 'action:' + placed['action_uuid'], '--reuse-installed', '--prepare-tool', '--expected-type', building['type'], '--expected-variant', building['variant']]
    return subprocess.run(command, cwd=ROOT).returncode

if __name__ == '__main__': raise SystemExit(main())
