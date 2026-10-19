"""Publish confirmed native UI placement and its separate Remove precondition."""
import json
import shutil
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
proof_id = '4c2fdea6-0957-4886-be37-5ca243f039b1'
summary = json.loads((ROOT / 'artifacts/acceptance' / proof_id / 'summary.json').read_text(encoding='utf-8'))
if summary.get('passed') is not True: raise RuntimeError('native UI placement proof failed')
images = []
for job in summary['jobs']:
    if job['step'] not in ('host-frame','client-1-frame'): continue
    evidence = json.loads(Path(job['evidence_path']).read_text(encoding='utf-8'))
    frame = evidence['calls'][-1]['result']['structuredContent']['result']
    target = ROOT / '_drafts/media' / ('normal_ui_build_' + job['step'] + '.bmp')
    shutil.copy2(frame['path'], target); images.append(target.name)
path = ROOT / '_drafts/normal_build_two.md'
value = path.read_text(encoding='utf-8')
marker = '\n## Aktuelle native Bilder\n'
if marker in value: value = value[:value.index(marker)]
value += marker + '\n[Neue native UI-Abnahme](../artifacts/acceptance/' + proof_id + '/summary.json) besteht einschließlich rollenabhängiger Diagnose. Beide Bilder wurden betrachtet: gleiches Gebäude und Geld94.200. Der Client bleibt im normalen kontinuierlichen Bautool mit nächster Preview.\n\n'
value += 'Der anschließende [Löschversuch](../artifacts/acceptance/afb2a43c-d7da-4a3c-b93d-29223948847d/summary.json) scheitert vor dem Remove-UICommand an der Auswahlvorbereitung bei aktivem Bautool. Keine Entfernung und keine Wiederholung. Der echte Toolwechsel wird ergänzt.\n\n'
for image in images: value += '![Normale Platzierung](media/' + image + ')\n\n'
path.write_text(value, encoding='utf-8')
for name in ('_drafts/progress.html','tools/update_progress.py'):
    target = ROOT / name; content = target.read_text(encoding='utf-8')
    section = '<section id="normal-ui-build"><h2>Normale gemeinsame Platzierung</h2><p>Native Client-Platzierung bestätigt, beide Geld94.200. Toolwechsel für anschließendes Löschen und Move-Hook-Anschluss sind in Arbeit.</p>'
    for image in images: section += '<img src="media/' + image + '" alt="Gasthaus nach gemeinsamer Client-Platzierung">'
    section += '</section>'
    if '<section id="normal-ui-build">' in content:
        start = content.index('<section id="normal-ui-build">'); end = content.index('</section>', start) + len('</section>')
        content = content[:start] + section + content[end:]
    else: content = content.replace('</main>', section + '</main>')
    target.write_text(content, encoding='utf-8')
print('info: native UI placement frames and separate Remove blocker published')
