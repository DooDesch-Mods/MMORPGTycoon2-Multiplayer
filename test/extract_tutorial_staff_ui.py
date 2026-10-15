from pathlib import Path
import argparse
p=argparse.ArgumentParser();p.add_argument('--source-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
f=a.source_root/'src/game/tutorial_startingpoint_fixture'
tabs=(f/'research_ui_read.inc').read_text();tabs=tabs[:tabs.index('bool research_measure(')]
same=(f/'client_research_fixture.inc').read_text();same=same[:same.index('bool client_research(')]
a.output.mkdir(parents=True,exist_ok=True);(a.output/'staff_common_extracted.inc').write_text(tabs+same)
