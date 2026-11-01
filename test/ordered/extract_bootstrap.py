"""Extract the current production bootstrap protocol, keeping only I/O doubled."""
import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = args.source / 'src/net/ordered_snapshot_load.inc'
body = source.read_text(encoding='utf-8-sig')

def position(marker):
    if body.count(marker) != 1:
        raise RuntimeError(f'{source}: expected one extraction boundary {marker!r}')
    return body.index(marker)

begin = position('static std::string ordered_bootstrap_digest(')
io_begin = position('static void ordered_export_covered(')
io_end = position('static bool ordered_receive_bootstrap(')
end = position('bool read_ordered_initial_bootstrap(')
if not begin < io_begin < io_end < end:
    raise RuntimeError(f'{source}: bootstrap extraction boundary order changed')
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(body[begin:io_begin] + body[io_end:end], encoding='utf-8', newline='\n')
