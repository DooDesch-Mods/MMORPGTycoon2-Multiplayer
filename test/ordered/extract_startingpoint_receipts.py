"""Extract the actual retained Starting Points receipt and its ledger serializer."""
import argparse
from pathlib import Path

def extract(source, begin, end):
    body=source.read_text(encoding='utf-8-sig')
    if body.count(begin)!=1 or body.count(end)!=1:
        raise RuntimeError(f'{source}: exact receipt extraction boundaries changed')
    a,b=body.index(begin),body.index(end)
    if a>=b:raise RuntimeError(f'{source}: receipt extraction boundary order changed')
    return body[a:b]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    runtime=args.source/'src/game/typed_action_runtime.cpp'
    body=runtime.read_text(encoding='utf-8-sig')
    if body.count('{"startingpoint_operations",runtime->engine.startingpoint_operations()}')!=1:
        raise RuntimeError('Starting Points retained receipts are not published through runtime status')
    members=extract(runtime,'    struct CommitOperation {','    static bool research_action(')
    method=extract(runtime,'    json startingpoint_operations()const{','    json building_move_receipt(')
    ledger=extract(args.source/'src/game/typed_action/action_session.cpp','json ledger_json(','\nLedger ledger_read(')
    ledger=ledger.replace('json ledger_json(','nlohmann::json management_ledger_observation(',1)
    args.output.mkdir(parents=True,exist_ok=True)
    for name,text in [('startingpoint_receipt_members.inc',members),('startingpoint_receipt_method.inc',method),
                      ('startingpoint_receipt_ledger.inc','namespace mt2mp::action_session {\n'+ledger+'\n}\n')]:
        (args.output/name).write_text(text,encoding='utf-8',newline='\n')

if __name__=='__main__':main()
