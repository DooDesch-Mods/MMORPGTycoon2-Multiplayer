from pathlib import Path
import argparse

p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);a=p.parse_args()
expected={
    'src/game/placement_ui_probe/ui_probe.cpp':('QueryPlacement',1),
    'src/game/native_open_witness.cpp':('QueryOpenWitness',2),
    'src/game/native_engine.cpp':('QueryEngine',3),
    'src/game/root_saved_building.h':('QuerySavedFields',1),
    'src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp':('QueryWorldProp',1),
}
for path,(category,count) in expected.items():
    text=(a.root/path).read_text(encoding='utf-8')
    assert text.count('apply_timing::virtual_query(')==count,path
    assert text.count('apply_timing::Part::'+category)==count,path
    position=0
    for unused in range(count):
        start=text.index('apply_timing::virtual_query(',position)
        end=text.index('(',start)+1;depth=1
        while depth:
            depth+=(text[end]=='(')-(text[end]==')');end+=1
        assert 'apply_timing::Part::'+category in text[start:end],path
        position=end
print('Eight actual native query call sites have exact adapter categories')
