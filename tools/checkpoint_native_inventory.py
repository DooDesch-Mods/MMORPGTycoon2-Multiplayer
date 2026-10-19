"""Refresh the current work item from retained native inventory evidence."""
import argparse
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--acceptance',required=True)
    args = parser.parse_args()
    path = root / 'artifacts/acceptance' / args.acceptance / 'summary.json'
    summary = json.loads(path.read_text(encoding='utf-8'))
    host = json.loads((root/'_MPTest/host/instance.json').read_text(encoding='utf-8'))
    client = json.loads((root/'_MPTest/client-1/instance.json').read_text(encoding='utf-8'))
    if host['run_id'] != client['run_id']:
        raise RuntimeError('Owned host and client manifests refer to different runs')
    dll_hash = hashlib.sha256((root/'build/bin/Release/MT2_Multiplayer.dll').read_bytes()).hexdigest()
    entry = (f'SDL3-Port, früher PhysFS-Pfadhook, Steam-Proxy, Loopback-Relay, eigene Peer-IDs, native Build-Brücke und stdio-MCP mit dem offiziellen SDK. '
             f'Aktueller Lauf `{host["run_id"]}`: Host PID {host["pid"]}, Starttoken {host["process_start_time"]}; '
             f'Client PID {client["pid"]}, Starttoken {client["process_start_time"]}. '
             f'Native Inventory-Abnahme `{args.acceptance}`: `{summary["state"]}`, bestanden={summary["passed"]}. '
             f'DLL-SHA256 `{dll_hash}`. `instance.json` und `_MPTest/run.json` bestimmen die genaue Prozessidentität. '
             'Beide Welten sind pausiert. Gesamter Koop ist weiterhin unbewiesen.')
    record = root/'_drafts/implementation_state.md'
    text = record.read_text(encoding='utf-8')
    first = text.index('SDL3-Port, früher PhysFS-Pfadhook')
    end = text.index('\n\n',first)
    text = text[:first]+entry+text[end:]
    evidence = ('\n\nNATIVE-INVENTORY-1: [Katalogfehler](../artifacts/acceptance/dcb4697c-68f9-40a7-b536-0050f79aaceb/summary.json) '
                'nach bestätigtem Host-Load, vor Save oder Client-Load. Slot0x13df440 ist ShardManager, nicht mmoWorld; '
                'der erste neue Reader kombinierte das korrekte ViewShard-Feld+60 mit dem falschen World-Vtable-Guard. '
                'Die ursprüngliche Hypothese bleibt verworfen. Separate, echte [Zeititem-Prüfung](../artifacts/acceptance/7512b5c6-3677-4b60-bb4d-d521ac558c22/summary.json) '
                'besteht nach einer neuen eigenen Host-Datei und bestätigtem Client-Load. Vier eindeutige Items mit Skalen0/1/3/9 und State1, '
                'keine Hooks oder Stores. Native Bilder betrachtet: Uhr23:22:56,129.178 Subscriber,8.967.530 Geld,5.600 Kapazität auf beiden Peers. '
                'Der sichtbare Pausebutton ist orange; Item-State ist kein belegter HUD-Highlightzustand. '
                'MCP bewahrt jetzt fehlgeschlagene native Teilquittungen unter structuredContent.native_response; '
                '[SDK55/55](../artifacts/mcp_checks/20261012T014312169436Z/result.json). Nächster Kandidat korrigiert den ShardManager-Receiver, '
                'behält Offset und Mitgliedschaftsprüfungen. Root/ViewShard-Abhängigkeit bleibt ein weiterer Blocker für unabhängige Dungeonkameras.')
    if 'NATIVE-INVENTORY-1: [Katalogfehler]' not in text:
        text = text[:first+len(entry)]+evidence+text[first+len(entry):]
    record.write_text(text,encoding='utf-8')
    print(f'Current native inventory checkpoint retained: {record}')


if __name__ == '__main__':
    main()
