from pathlib import Path
import argparse

parser=argparse.ArgumentParser()
parser.add_argument('--root',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
def body(text,anchor):
    start=text.index(anchor);signature=text.index('(',start)+1;parens=1
    while parens:
        parens+=(text[signature]=='(')-(text[signature]==')');signature+=1
    end=text.index('{',signature)+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
scope=(args.root/'src/game/subscriber_lifecycle/save_scope.cpp').read_text()
(args.output/'save_scope_actual.inc').write_text('\n'.join(body(scope,a) for a in [
    'bool callback(','bool begin_save_scope(','bool finish_save_scope(','bool with_save_scope('])+'\n')
runtime=(args.root/'src/game/subscriber_lifecycle/runtime_append.cpp').read_text()
(args.output/'quest_wrappers_actual.inc').write_text('\n'.join(body(runtime,a) for a in [
    'static bool bind_quest_instance_impl(','bool progress_apply(',
    'bool bind_quest_instance(const QuestLinkRequest&','bool bind_quest_instance(QuestEditBatch&',
    'bool ensure_progress(const ProgressRequest&','bool retire_progress(const ProgressRequest&',
    'bool ensure_progress(QuestEditBatch&','bool retire_progress(QuestEditBatch&'])+'\n')
print('Extracted actual Quest helper wrappers and SaveScope lifecycle')
