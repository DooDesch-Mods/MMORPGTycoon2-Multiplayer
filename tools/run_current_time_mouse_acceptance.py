"""Run new native mouse checks from the measured preparation receipt."""
from pathlib import Path
import argparse,json,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--setup',type=Path,required=True)
setup=json.loads(parser.parse_args().setup.read_text(encoding='utf-8'))
if setup.get('passed') is not True:raise ValueError('time mouse setup is not confirmed')
print('info: native mouse candidate measured DLL='+setup['built_dll_sha256']+' run='+setup['new_run_id'],flush=True)
raise SystemExit(subprocess.run([sys.executable,'-X','utf8',str(ROOT/'tools/time_mouse_acceptance.py'),
    '--expected-run-id',setup['new_run_id'],'--expected-dll-sha256',setup['built_dll_sha256']],cwd=ROOT).returncode)
