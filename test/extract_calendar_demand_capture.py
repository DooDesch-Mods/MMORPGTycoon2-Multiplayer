from pathlib import Path
import argparse,hashlib,json

p=argparse.ArgumentParser()
p.add_argument('--root',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
native=(a.root/'src/game/character_calendar_native.cpp').read_text(encoding='utf-8')
def section(start,end):
    assert native.count(start)==1 and native.count(end)==1,(start,end)
    return native[native.index(start):native.index(end)]
outputs={
    'calendar_demand_owners_actual.inc':section('struct DemandOwner {','struct Memory {'),
    'calendar_demand_capture_actual.inc':section('bool capture_demands(','bool apply(const a::Context& context,')}
for name,body in outputs.items():(a.output/name).write_text(body,encoding='utf-8')
capture=outputs['calendar_demand_capture_actual.inc']
assert capture.count('once(memory,context,binding,')==2
assert 'same_bound(first,second,error)' in capture and 'before!=after' in capture
assert 'first_owners!=second_owners||first_values!=second_values' in capture
assert 'subscriber_demand::reciprocal(second_values,after,error)' in capture
assert capture.index('port.current_owners(second.pin,error)')<capture.index('out=std::move(after)')
assert 'demand_keys(' not in capture and 'static ' not in capture and 'thread_local' not in capture
management=(a.root/'src/game/management_live_port.inc').read_text(encoding='utf-8')
sample=management[management.index('bool calendar_sample('):management.index('bool primary_once(')]
assert sample.count('character_calendar::capture_demands(')==1
assert 'subscriber_demand_native::sample(' not in sample and 'character_calendar::capture(' not in sample
assert 'PageBoundary::OriginalCall' in sample
assert 'subscriber_demand_native::apply(' in management and 'character_calendar::apply(' in management
receipt={'state':'PASS','functions':['same_bound','capture_demands'],
         'source_sha256':hashlib.sha256((a.root/'src/game/character_calendar_native.cpp').read_bytes()).hexdigest(),
         'management_sha256':hashlib.sha256((a.root/'src/game/management_live_port.inc').read_bytes()).hexdigest(),
         'generated':{name:hashlib.sha256(body.encode()).hexdigest() for name,body in outputs.items()}}
(a.output/'calendar_demand_capture.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('Calendar/Demand actual paired capture and owner comparisons extracted')
