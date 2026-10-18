"""Extract the actual lifecycle helper and attempted-effect loop for its fixed fixture."""
import argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--dependencies',type=Path)
a=p.parse_args();base='src/game/live_host_state/day_rollover/economy/'
t=(a.root/(base+'native_online.inc')).read_text()
end=t.index('bool NativePort::preflight_lifecycle(')
assert t[:end].count('template<class Move>bool online_apply(')==1
a.output.mkdir(parents=True,exist_ok=True)
(a.output/'online_sample.inc').write_text(t[:end])
v=((a.dependencies or a.root)/(base+'monetary.cpp')).read_text()
needle='if(old.has_value()==after.has_value())continue;receipt.attempted=true;++receipt.attempted_stores;'
start=v.rfind('        for(size_t i=0;i<before.subscribers.size();++i){',0,v.index(needle))
end=v.index('\n        }',v.index(needle))+len('\n        }')
loop=v[start:end]
assert loop.count('port.store_lifecycle(')==1 and loop.index(needle)<loop.index('port.store_lifecycle(')<loop.index('++receipt.stores;')
assert 'stop("actual Subscriber native lifecycle Unknown");return receipt;' in loop
(a.output/'lifecycle_loop.inc').write_text(loop+'\n')
