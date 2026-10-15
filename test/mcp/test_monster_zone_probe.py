from pathlib import Path
import sys,copy,asyncio
r=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(r/'tools/mcp'))
import monster_zone_probe_ops as ops
from runtime import RuntimeErrorDetail
u='11111111-2222-4333-8444-555555555555'
a=dict(token=u,snapshot_uuid=u,expected_world_epoch=1,expected_generation=1,region_id=67,phase='open',character_type=-1,points=[])
count=0
for phase in ('open','state','select','stroke'):
 b=copy.deepcopy(a);b['phase']=phase
 if phase in ('select','stroke'):b['character_type']=0
 if phase=='stroke':b['points']=[[1,2,3],[4,5,6]]
 ops.validate('native_monster_zone_probe',b);count+=1
bad=[{},dict(a,extra=1),dict(a,token='invalid'),dict(a,snapshot_uuid='invalid')]
for key in ('expected_world_epoch','expected_generation','region_id','character_type'):
 for value in (True,1.5,-2,2**64):bad.append(dict(a,**{key:value}))
bad += [dict(a,phase='arbitrary'),dict(a,phase='stroke',character_type=0,points=[[1,2,3]]),dict(a,phase='stroke',character_type=-1,points=[[1,2,3],[1,2,3]]),dict(a,phase='select'),dict(a,points=[[1,2,3]])]
for value in (True,float('nan'),float('inf'),1e39):bad.append(dict(a,phase='stroke',character_type=0,points=[[value,0,0],[1,2,3]]))
for b in bad:
 try:ops.validate('native_monster_zone_probe',b)
 except RuntimeErrorDetail as e:assert e.code=='invalid_args'
 else:raise AssertionError(('accepted invalid',b))
 count+=1
import server,runtime
assert 'native_monster_zone_probe' in server.OP_SCHEMAS
assert 'native_monster_zone_probe' in runtime.OPS and 'native_monster_zone_probe' in runtime.MUTATIONS
assert server.OP_SCHEMAS['native_monster_zone_probe']==ops.SCHEMAS['native_monster_zone_probe']
print(count,'actual adapter validation cases plus server/runtime wiring PASS')
