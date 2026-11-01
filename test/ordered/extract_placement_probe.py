"""Extract the production placement operation and its original public wrappers."""
from pathlib import Path
import argparse

def block(text,start,end):
    if text.count(start)!=1 or text.count(end)!=1:raise RuntimeError('Placement extraction markers changed')
    a,b=text.index(start),text.index(end)
    if a>=b:raise RuntimeError('Placement extraction marker order changed')
    return text[a:b]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();text=(args.source/'src/game/native_engine.cpp').read_text(encoding='utf-8-sig')
    result=block(text,'struct PlacementResult {','\nstruct IslandProbe {')
    valid=block(text,'bool valid_test_name(const std::string& name) {','\n}\n}\n\nbool initialize(')+'\n}\n'
    operation=block(text,'static bool placement_operation(','\nbool read_world_status(')
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'placement_result.inc').write_text(result,encoding='utf-8',newline='\n')
    (args.output/'placement_probe_actual.inc').write_text(valid+'\n'+operation,encoding='utf-8',newline='\n')

if __name__=='__main__':main()
