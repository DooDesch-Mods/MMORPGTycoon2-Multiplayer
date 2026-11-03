from pathlib import Path
import sys
source,output=map(Path,sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
text=(source/'src/game/ability_membership_runtime.inc').read_text()
icon=text[text.index('bool icon('):text.index('bool queue(')]
dock=text[text.index('void dock_entry(void* receiver'):text.index('\n}\nbool pending_ui()',text.index('void dock_entry(void* receiver'))]
(output/'ability_presentation_actual.inc').write_text(icon+dock+'\n',newline='\n')
text=(source/'src/game/typed_action_runtime.cpp').read_text()
binding=text[text.index('static bool read_binding_impl('):text.index('bool observe_startingpoint_submission(')]
(output/'ability_presentation_binding_actual.inc').write_text(binding,newline='\n')
