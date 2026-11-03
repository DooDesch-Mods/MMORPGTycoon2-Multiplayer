from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
def save(name,text):(a.output/name).write_text(text,encoding='utf-8',newline='\n')
quest=(a.source/'src/game/npc_quest_list_runtime.inc').read_text()
start=quest.index('bool capture(bool remove');cut=quest.index('    const auto controller=',start);end=quest.index('bool npc_entry(',cut);tail=quest.index('}catch(const std::exception& x)',cut)
save('quest_command_gate_actual.inc',quest[quest.index('const char* edit_label('):quest.index('bool edit_args(')]+quest[start:cut]+'    ++selected;selected_edit=editing;return true;\n'+quest[tail:end])
region=(a.source/'src/game/region_styles_runtime.inc').read_text()
save('selection_label_actual.inc',region[region.index('uint32_t selection_label_is('):region.index('bool appearance_capture(')])
region=(a.source/'src/game/region_appearance_runtime.inc').read_text();start=region.index('bool appearance_capture(');cut=region.index('    if(!property_capture_admit(',start)
save('appearance_command_gate_actual.inc',region[start:cut]+'    (void)window;++selected;return true;\n}\n')
tab=(a.source/'src/game/dungeon_config_ui.inc').read_text()
save('dungeon_tab_command_actual.inc',tab[tab.index('bool tab_record_equals('):tab.index('uint32_t find_control(')])
print('Extracted exact Quest/Region admission gates and native label/tab helpers; unchanged downstream model validation excluded')
