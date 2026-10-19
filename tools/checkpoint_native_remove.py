"""Record completed native removal without repeating game actions."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IDENTIFIER = 'c71af124-2a7a-4c3f-9313-f73187ee8eb3'
DIRECTORY = ROOT / 'artifacts/acceptance' / IDENTIFIER


def main():
    summary_path = DIRECTORY / 'summary.json'
    summary = json.loads(summary_path.read_text(encoding='utf-8'))
    if summary.get('passed') is not True or summary.get('state') != 'confirmed-native-remove-two':
        raise ValueError('native removal is not confirmed')
    removed = [entry['result'] for entry in summary['checks'] if entry['check'] == 'native-remove-confirmed']
    absent = [entry['result'] for entry in summary['checks'] if entry['check'] == 'native-target-absent']
    if {item['peer'] for item in removed} != {'host', 'client-1'} or len(removed) != 2 or len(absent) != 2:
        raise ValueError('native two-peer removal coverage incomplete')
    for item in removed:
        receipt = item['receipt']
        if receipt['confirmed'] is not True or receipt['binding_revoked'] is not True or receipt['result']['retirement_confirmed'] is not False:
            raise ValueError('native removal effect/retirement scope differs')
    if absent[0]['ledger'] != absent[1]['ledger'] or absent[0]['ledger']['cash'] != 95000:
        raise ValueError('native refund ledger differs')
    frame_hashes = {}
    for phase in ('before', 'after'):
        path = DIRECTORY / f'frame_checkpoint_{phase}.json'
        record = json.loads(path.read_text(encoding='utf-8'))
        if record['frame_phase'] != phase or record['game_operations'] != 0 or set(record['frames']) != {'host', 'client-1'}:
            raise ValueError('native frame phase coverage differs')
        for item in record['frames'].values():
            if hashlib.sha256(Path(item['retained_path']).read_bytes()).hexdigest() != item['file_sha256']:
                raise ValueError('retained native frame changed')
        frame_hashes[phase] = hashlib.sha256(path.read_bytes()).hexdigest()
    text = f'''# Native SmallInn-Entfernung auf zwei Instanzen

Die SmallInn ist auf Host und Client jeweils nach genau einem eigenen nativen DestroyProp verschwunden. Eigener Graph/Dtor, kontinuierlich gehaltenes WeakNULL und erhaltene leere Auswahl sind bestätigt. Beide Ledger zeigen95.000 statt94.600 Geld: die Engine erstattet400. [Abnahme](../artifacts/acceptance/{IDENTIFIER}/summary.json).

Vier native Nahansichten wurden betrachtet: [vorher](../artifacts/acceptance/{IDENTIFIER}/frame_checkpoint_before.json) und [nachher](../artifacts/acceptance/{IDENTIFIER}/frame_checkpoint_after.json). Das Gebäude fehlt nachher, der Baum bleibt. Beide zeigen den Undo-Eintrag „Destroy Small Inn #1“ und das zurückgesetzte Bauziel0/3. Uhr00:00:28 und übrige Oberfläche bleiben erhalten. Undo-Ausführung und eine Auswahl mit mehreren überlebenden Objekten sind damit noch nicht geprüft.

Aktiver Lauf `{summary['runtime_run_id']}`, DLL `{summary['parameters']['expected_dll_sha256']}`. [Produktionskopie](../artifacts/remove_integration/20261012T114529930492Z/copy_receipt.json) und [korrigierter Build](../artifacts/build_jobs/native_remove_fresh_binding_20261012/status.json). [Zwölf Wrapper- und vier SDK-Gruppen](remove_diagnostics_tests.md) prüfen die neue Bindung, Quittungen und Wiederholungssperren.

Dieser native Zustand wird übernommen. Es gibt je einen gehaltenen Tombstone-Holder; seine sichere Freigabe ist die nächste Prüfung. Beide Löschungen wurden einzeln über MCP ausgelöst. Ein Client-Auftrag über Host und bestätigte Replikation bleibt offen. Danach folgen regulärer Place-Commit mit Scenery/Undo und der vollständige gemeinsame Aktionsweg. Laufende Wirtschaft, Events, vollständige Simulation und Steam sind weiterhin offen; vier Instanzen und großer Save folgen final.
'''
    (ROOT / '_drafts/native_remove_two.md').write_text(text, encoding='utf-8')
    checkpoint = DIRECTORY / 'native_remove_checkpoint.json'
    if checkpoint.exists():
        raise ValueError('native Remove checkpoint is immutable')
    checkpoint.write_text(json.dumps({'passed': True, 'summary_sha256': hashlib.sha256(summary_path.read_bytes()).hexdigest(),
        'frame_checkpoint_sha256': frame_hashes, 'runtime_run_id': summary['runtime_run_id'],
        'native_remove_proven': True, 'native_refund_each': 400,
        'client_request_replication_proven': False, 'holder_retirement_proven': False}, indent=2) + '\n', encoding='utf-8')
    path = ROOT / '_drafts/implementation_state.md'
    state = path.read_text(encoding='utf-8')
    start = state.index('## Aktueller nativer Löschkandidat')
    end = state.index('## Aktueller MCP-Eingabekandidat', start)
    current = f'''## Aktueller nativer Löschkandidat

[NativeRemove2 bestanden](native_remove_two.md): `{IDENTIFIER}`, aktiver Lauf `{summary['runtime_run_id']}`, DLL `{summary['parameters']['expected_dll_sha256']}`. Je einmal SmallInn nativ zerstört, eigener Dtor/Graph/continuous WeakNULL und leere Auswahl bestätigt. Beide Geld94.600->95.000, gleiche20 Buchungskategorien. Vier Vorher/Nachher-Bilder betrachtet, einzeln hashgebunden erhalten. Undo zeigt Destroy Small Inn #1; Bauziel1/3->0/3. Dieser Zustand wird übernommen, kein unverändertes Gesamt-Retest. [NativeFree2](native_free_two.md), frühere Persistenz und Zeit/HUD bleiben bestanden.

Root besitzt Produktion/Build/MCP. NeueWrapper/SDK12/4 bestanden, historischeAcceptance36+Free12/4/8 klar getrennt erhalten. Rootbindings prüfen Hostcapture jetzt über frischen seal-Worldwitness; ClientpositiveNativeLinkID wird mit geprüfterDateiSHA zu canonicalRecordKey. Ursprünglicher Build91de und Setups3309017f/3915f957 sind nur Vorbereitung, keine weitere Löschung. NeueBuildab91 und Setup5c41 habenc71 ermöglicht. ProPeer einTombstoneHolder bleibt gehalten, kein abschließenderFreeACK. NächsterSchritt: RetirementdeltaALLreview+actualRootBindingRevocationProvider integrieren, actualperallocationQuiescence/FreeACK prüfen. Agentengine_host_delta besitztStaging retirement_delta; Agentaction_session erstellt gemeinsamenHost/ClienttypedActioncontroller channel9; RootnativeBridge bleibtRootaufgabe. KeinComputerUse, nurMCP; zweiInstanzen,4/großerSavefinal. FullPlace/Scenery/Undo, breiteAuswahlfälle/Objektgruppen, laufendeWirtschaft/Events/Simulation/Steam offen.

'''
    path.write_text(state[:start] + current + state[end:], encoding='utf-8')
    print(f'info: native Remove checkpoint={checkpoint}; no game effects')


if __name__ == '__main__':
    main()
