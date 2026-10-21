from pathlib import Path
import sys

source,destination=map(Path,sys.argv[1:3])
text=source.read_text(encoding='utf-8')
destination.mkdir(parents=True,exist_ok=True)
for begin,end,name in (
    ('bool graph(Memory&','bool protected_others(','monster_history_graph_body.inc'),
    ('bool protected_others(','bool proof(','monster_history_others_body.inc'),
):
    assert text.count(begin)==1 and text.count(end)==1,(begin,end)
    start=text.index(begin);stop=text.index(end,start)
    (destination/name).write_text(text[start:stop],encoding='utf-8',newline='\n')
print('Extracted two actual Monster history bodies')
