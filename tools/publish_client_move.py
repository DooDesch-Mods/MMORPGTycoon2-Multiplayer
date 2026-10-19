"""Publish the retained confirmed native Client Move and viewed framebuffers."""
from pathlib import Path
import json
import shutil
ROOT=Path(__file__).resolve().parents[1]
proof=json.loads((ROOT/'artifacts/move_retained/client_20261012/summary.json').read_text(encoding='utf-8'))
view=json.loads((ROOT/'artifacts/acceptance/c12965d9-0dcf-4f56-b28b-9dbe71f7ab06/summary.json').read_text(encoding='utf-8'))
if not proof.get('passed') or not view.get('passed') or proof['runtime_run_id']!=view['runtime_run_id']: raise RuntimeError('native Move proof/view differs')
images=[]
for job in view['jobs']:
    if 'frame' not in job['step']: continue
    evidence=json.loads(Path(job['evidence_path']).read_text(encoding='utf-8'))
    frame=evidence['calls'][-1]['result']['structuredContent']['result']
    target=ROOT/'_drafts/media'/('client_move_'+job['step']+'.bmp'); shutil.copy2(frame['path'],target); images.append(target.name)
text='''# Originaler Client-Move auf zwei Instanzen

[Tatsächliche native Belege](../artifacts/move_retained/client_20261012/summary.json) bestätigen den Clientauftrag b6c04e10-6da4-487a-b31c-7d336067f062 auf beiden Instanzen. Laufd0563a55-ac48-4e55-b61f-ccf6707fab33, DLL4f5711af96e8b2eb0e306816cbad15904230bbd9a3fd4cfabeaf30c3f82a58d1.

SmallInn/base behält UID1086 und denselben gemeinsamen Schlüssel. Originaler DoMove, echter inverser Undo-Graph und native Aufräumarbeiten sind bestätigt. Geld94.600 und alle20 Buchungsgruppen bleiben gleich. Host bestätigt beide ACKs, keine Duplikate oder fehlgeschlagenen Aktionen. Position7990/63,5814094543457/8550 stimmt auf beiden Instanzen überein.

Die originale [Abnahme](../artifacts/acceptance/afdafb88-4cae-49ac-a560-629b4e71a05b/summary.json) bleibt als fehlgeschlagene Diagnose erhalten: Sie erwartete auch nach dem nächsten Weltupdate noch Previewhöhe63,75876998901367. Der echte Move war bereits vollständig bestätigt. Die Folgeprüfung berücksichtigt native Bodenhöhe und wurde ausschließlich auf erhaltene Belege angewendet; kein Move-Replay.

Beide [nativen Bilder](../artifacts/acceptance/c12965d9-0dcf-4f56-b28b-9dbe71f7ab06/summary.json) wurden betrachtet. Sie zeigen dasselbe Gasthaus, Geld94.600 und Undoeintrag Move. Der originale Move wählt das betroffene Objekt auch beim Empfänger aus. Unabhängige lokale Auswahl während fremder Moves ist noch offen.

[Beide vollständigen lokalen Saves](../artifacts/acceptance/de403ddb-6e57-4017-8dee-dc2263bca738/summary.json) erhalten den aktuellen Zustand. Host-Auftrag und normale Move-Oberflächenaktion sind die nächsten separaten Nachweise. Die Höhenzuordnung eines bewegten Bestandsobjekts wird ergänzt.

'''
for image in images: text+='![Client-Move auf beiden Instanzen](media/'+image+')\n\n'
(ROOT/'_drafts/client_move_two.md').write_text(text,encoding='utf-8')
for name in ('_drafts/progress.html','tools/update_progress.py'):
    path=ROOT/name; content=path.read_text(encoding='utf-8')
    section='<section id="client-native-move"><h2>Gemeinsames Verschieben</h2><p>Client-Move bestätigt auf beiden Instanzen: gleiches Objekt, Geld94.600 unverändert, echter Undo und Host-ACK3. <a href="client_move_two.md">Native Belege</a>. Normale Move-Oberfläche und Host-Auftrag werden geprüft.</p>'
    for image in images: section+='<img src="media/'+image+'" alt="Gasthaus nach gemeinsamem Verschieben">'
    section+='</section>'
    if '<section id="client-native-move">' not in content: content=content.replace('</main>',section+'</main>')
    path.write_text(content,encoding='utf-8')
print('info: actual Client Move proof and viewed native frames published')
