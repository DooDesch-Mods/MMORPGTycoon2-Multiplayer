"""Update the live local progress notice and its source template."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
old = '<p class="open"><strong>Aktuelle native Bauprobe:</strong>'
notice = '<p class="open"><strong>Aktuelle native Bauprobe:</strong> Die fehlende originale Filterinitialisierung ist integriert. Der Host hat ein SmallInn mit Purchase/Add/Undo jeweils einmal und Kosten400 erzeugt. Die native Bodenanpassung wurde als endgültiges Host-Ergebnis berücksichtigt; die neue Zweier-Abnahme läuft. <a href="implementation_state.md">Aktueller Beleg und offene Schritte</a>. Normale gemeinsame UI-Platzierung bleibt noch unbestätigt.</p>'
for name in ('_drafts/progress.html', 'tools/update_progress.py'):
    path = ROOT / name
    content = path.read_text(encoding='utf-8')
    start = content.index(old)
    end = content.index('</p>', start) + len('</p>')
    path.write_text(content[:start] + notice + content[end:], encoding='utf-8')
print('info: native progress notice updated')
