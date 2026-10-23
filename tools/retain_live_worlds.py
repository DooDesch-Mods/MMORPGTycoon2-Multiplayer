"""Retain both exact owned worlds through local-only native game MCP saves."""
import argparse
from pathlib import Path
import time
import uuid
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, validate_snapshot, hash_file, within, utc, log, extract_result
from runtime import FRAMEWORK_PLUGIN_KIND, manifest_runtime_kind, candidate_binary_receipt


def _owned_data_root(manifest, peer_root, run_id):
    if Path(manifest['runtime_root']).resolve() != peer_root.resolve() or manifest.get('owner') != {'kind': 'MPTest', 'run_id': run_id}:
        raise AcceptanceError('Local save manifest owner/root differs')
    if manifest_runtime_kind(manifest) != FRAMEWORK_PLUGIN_KIND:
        return within(peer_root / 'data', peer_root)
    profile = within(manifest['framework_profile'], peer_root)
    if profile != (peer_root / 'profile').resolve():
        raise AcceptanceError('Framework save requires exact owned profile')
    return within(profile / 'game-data', peer_root)


def verify_local_file(record, manifest, peer_root, run_id):
    """Validate an acknowledged local-only save inside the exact owned game-data root."""
    if record.get('state') != 'snapshot_file_complete' or record.get('distribution_requested') is not False or record.get('run_id') != run_id:
        raise AcceptanceError('Local save completion/distribution/run receipt differs; no save replay')
    uuid.UUID(record['snapshot_id'])
    data = _owned_data_root(manifest, peer_root, run_id)
    if manifest_runtime_kind(manifest) != FRAMEWORK_PLUGIN_KIND:
        return validate_snapshot(record, record['snapshot_id'], peer_root)
    path = within(record['path'], data)
    parts = path.relative_to(data.resolve()).parts
    if parts and parts[0].isdecimal() and len(parts[0]) <= 20:
        parts = parts[1:]
    if len(parts) != 3 or parts[0] != 'save' or path.suffix != '.vrz':
        raise AcceptanceError('Framework snapshot path must be game-data/[account]/save/world/file.vrz')
    length, digest = record.get('bytes'), record.get('sha256')
    if type(length) is not int or not 0 < length <= 1024 * 1024 * 1024 or not isinstance(digest, str) or len(digest) != 64:
        raise AcceptanceError('Local save size/hash receipt invalid')
    if path.stat().st_size != length or hash_file(path) != digest:
        raise AcceptanceError('Local save complete file differs from native size/hash receipt; no save replay')
    return {**record, 'path': str(path), 'file_verified': True}


def _healthy(engine, peer):
    action = engine.get('typed_actions', {})
    live = action.get('live_state', {})
    if (action.get('started') is not True or action.get('live_failure_terminal') is not False
            or action.get('locked') is not False or action.get('pending') is not False
            or live.get('active') is not True or live.get('locked') is not False
            or live.get('unknown') is not False or live.get('economic_fence_locked') is not False):
        raise AcceptanceError(f'Save overlap requires healthy settled {peer}: {live.get("cause") or action.get("cause") or "missing/failed state"}')
    origin = engine.get('native_open_origin', {})
    keys = ('uuid', 'generation', 'world_epoch', 'origin_world_epoch')
    if any(key not in origin for key in keys):
        raise AcceptanceError('Save overlap lacks exact current world identity: ' + peer)
    return {key: origin[key] for key in keys}


def _wait_receipt(engine):
    receipt = engine.get('typed_actions', {}).get('live_state', {}).get('application_wait_receipt', {})
    if (type(receipt.get('waiting')) is not bool
            or any(type(receipt.get(key)) is not int or receipt[key] < 0 for key in ('observations', 'recoveries'))):
        raise AcceptanceError('Client application wait receipt absent or malformed')
    return dict(receipt)


def _client_ack(engine):
    peers = engine.get('typed_actions', {}).get('frame_transport', {}).get('apply_ack', {}).get('peers', [])
    rows = [row for row in peers if row.get('peer') == 2]
    if len(rows) != 1 or type(rows[0].get('applied_revision')) is not int or rows[0]['applied_revision'] < 1:
        raise AcceptanceError('Host lacks a confirmed Client Frame ACK baseline')
    return rows[0]['applied_revision']


def verify_client_save_wait(runner):
    """Return a confirmed receipt or raise with retained evidence; caller owns the clock."""
    manifests = {peer: read_json(runner.manifests[peer]) for peer in ('host', 'client-1')}
    for peer, manifest in manifests.items():
        _owned_data_root(manifest, runner.root / peer, runner.summary['runtime_run_id'])
    candidates = {peer: candidate_binary_receipt(manifest, runner.args.candidate_binary) for peer, manifest in manifests.items()}
    if candidates['host']['sha256'] != candidates['client-1']['sha256']:
        raise AcceptanceError('Save overlap peers use different candidates')
    baseline = {peer: runner.inspect('save-wait-before-' + peer, peer) for peer in manifests}
    origins = {peer: _healthy(engine, peer) for peer, engine in baseline.items()}
    before_wait = _wait_receipt(baseline['client-1'])
    if before_wait['waiting'] or baseline['host'].get('clock', {}).get('scale') not in (1, 3, 9):
        raise AcceptanceError('Save overlap requires running Host and no existing Client save wait')
    before_ack = _client_ack(baseline['host'])
    record = {'state': 'attempted', 'confirmed': False, 'save_requests': 1,
              'before_wait': before_wait, 'before_ack_revision': before_ack, 'candidate_binaries': candidates}
    runner.summary['client_save_overlap'] = record
    runner.persist()
    try:
        saved = runner.single('client-save-overlap-once', 'client-1', 'game_capture_local_snapshot')
        record['snapshot'] = verify_local_file(saved, manifests['client-1'], runner.root / 'client-1', runner.summary['runtime_run_id'])
        deadline = time.monotonic() + getattr(runner.args, 'wait_seconds', 30)
        while True:
            after = {peer: runner.inspect('save-wait-after-' + peer, peer) for peer in ('client-1', 'host')}
            for peer, engine in after.items():
                if _healthy(engine, peer) != origins[peer]:
                    raise AcceptanceError('World identity changed during local-only save: ' + peer)
            wait, ack = _wait_receipt(after['client-1']), _client_ack(after['host'])
            record.update(after_wait=wait, after_ack_revision=ack)
            if wait['observations'] < before_wait['observations'] or wait['recoveries'] < before_wait['recoveries'] or ack < before_ack:
                raise AcceptanceError('Save overlap receipt counters regressed')
            if wait['observations'] == before_wait['observations']:
                record.update(state='inconclusive-no-overlap', cause='One native save completed without an observed incoming Frame wait; save is not repeated')
                raise AcceptanceError(record['cause'])
            if wait['recoveries'] > before_wait['recoveries'] and not wait['waiting'] and ack > before_ack:
                record.update(state='confirmed-client-save-wait', confirmed=True)
                break
            if time.monotonic() >= deadline:
                raise AcceptanceError(f'Client save wait did not recover with newer Frame ACK: before={before_wait} after={wait} ack={ack} required>{before_ack}; no save replay')
            runner.persist()
        runner.checked('client-native-save-overlap', record)
        return record
    except Exception as error:
        if record['state'] != 'inconclusive-no-overlap':
            record.update(state='failed-no-replay', cause=f'{type(error).__name__}: {error}')
        runner.persist()
        raise

def _periodic_stamp(path):
    try:
        stat = path.stat()
    except FileNotFoundError:
        return None
    return {'bytes': stat.st_size, 'mtime_ns': stat.st_mtime_ns, 'file_id': stat.st_ino}


def observe_client_autosave_wait(runner, timeout_seconds=360):
    """Observe the first natural periodic save; never request a save or change the clock."""
    if not 1 <= timeout_seconds <= 360:
        raise AcceptanceError('Natural autosave observation requires 1..360 seconds')
    record = {'state': 'observing', 'confirmed': False, 'save_requests': 0, 'timeout_seconds': timeout_seconds}
    runner.summary['client_autosave_wait'] = record
    runner.persist()
    deadline = time.monotonic() + timeout_seconds
    read_budget = min(60, getattr(runner.args, 'step_timeout', 45) + 15)
    record['minimum_read_budget_seconds'] = read_budget

    class Expired(Exception):
        pass

    def inspect(name, peer):
        remaining = deadline - time.monotonic()
        # Admit only a full existing MCP job window. A clipped final request
        # would turn the observation deadline into a manufactured tool failure.
        if remaining < read_budget:
            record['stopped_remaining_seconds'] = max(0, remaining)
            raise Expired()
        receipt = runner.job(name, peer, [{'tool': 'game_inspect_engine', 'arguments': {}}], remaining=remaining)
        return extract_result(receipt, 'game_inspect_engine')

    try:
        manifests = {peer: read_json(runner.manifests[peer]) for peer in ('host', 'client-1')}
        roots = {peer: _owned_data_root(manifest, runner.root / peer, runner.summary['runtime_run_id']) for peer, manifest in manifests.items()}
        record['candidate_binaries'] = candidates = {peer: candidate_binary_receipt(manifest, runner.args.candidate_binary) for peer, manifest in manifests.items()}
        if candidates['host']['sha256'] != candidates['client-1']['sha256']:
            raise AcceptanceError('Autosave observer peers use different candidates')
        selected = getattr(runner.args, 'client_autosave_path', None)
        if selected is None:
            saved = runner.summary.get('client_save_overlap', {}).get('snapshot')
            if not saved:
                raise AcceptanceError('Natural autosave observer requires explicit active-world periodic path or verified local save receipt')
            selected = Path(verify_local_file(saved, manifests['client-1'], runner.root / 'client-1', runner.summary['runtime_run_id'])['path']).with_name('periodic.vrz')
        path = within(selected, roots['client-1'])
        parts = path.relative_to(roots['client-1']).parts
        if parts and parts[0].isdecimal() and len(parts[0]) <= 20:
            parts = parts[1:]
        if len(parts) != 3 or parts[0] != 'save' or parts[-1] != 'periodic.vrz':
            raise AcceptanceError('Natural autosave path must be exact owned save/world/periodic.vrz')
        record['periodic_path'] = str(path)
        initial_stamp = _periodic_stamp(path)
        record['before_file'] = initial_stamp
        baseline = {peer: inspect('autosave-before-' + peer, peer) for peer in manifests}
        origins = {peer: _healthy(value, peer) for peer, value in baseline.items()}
        before_wait, before_ack = _wait_receipt(baseline['client-1']), _client_ack(baseline['host'])
        record.update(before_wait=before_wait, before_ack_revision=before_ack)
        if before_wait['waiting'] or baseline['host'].get('clock', {}).get('scale') not in (1, 3, 9):
            raise AcceptanceError('Autosave observer requires running Host and no existing Client save wait')
        first_complete = None
        while True:
            before_read_stamp = _periodic_stamp(path)
            current = {peer: inspect('autosave-observe-' + peer, peer) for peer in ('client-1', 'host')}
            for peer, value in current.items():
                if _healthy(value, peer) != origins[peer]:
                    raise AcceptanceError('World identity changed during natural autosave observation: ' + peer)
            wait, ack = _wait_receipt(current['client-1']), _client_ack(current['host'])
            record.update(after_wait=wait, after_ack_revision=ack)
            if wait['observations'] < before_wait['observations'] or wait['recoveries'] < before_wait['recoveries'] or ack < before_ack:
                raise AcceptanceError('Natural autosave receipt counters regressed')
            saves = current['client-1'].get('native_world', {}).get('saves')
            if type(saves) is not int or saves < 0:
                raise AcceptanceError('Client native saves counter unavailable or invalid')
            stamp = _periodic_stamp(path)
            if first_complete is None and stamp is not None and stamp != initial_stamp and stamp == before_read_stamp and saves == 0:
                if not 0 < stamp['bytes'] <= 1024 * 1024 * 1024:
                    raise AcceptanceError('New periodic save file has invalid size')
                digest = hash_file(path)
                if _periodic_stamp(path) == stamp:
                    first_complete = stamp
                    record['periodic_file'] = {'state': 'native-idle-stable-file', 'snapshot_ack': False,
                                               'path': str(path), **stamp, 'sha256': digest}
                    record['first_complete_ack_revision'] = ack
                    if wait['observations'] == before_wait['observations']:
                        record.update(state='inconclusive-no-overlap', cause='First natural periodic save completed without an incoming Frame wait; no further save observed')
                        raise AcceptanceError(record['cause'])
            elif first_complete is not None and stamp != first_complete:
                raise AcceptanceError('First completed periodic file changed before recovery proof; no later save substitution')
            if (first_complete is not None and wait['recoveries'] > before_wait['recoveries']
                    and not wait['waiting'] and ack > record['first_complete_ack_revision']):
                record.update(state='confirmed-client-autosave-wait', confirmed=True)
                runner.checked('client-natural-autosave-overlap', record)
                return record
            runner.persist()
    except Expired:
        if 'periodic_file' not in record:
            record.update(state='inconclusive-no-periodic-save', cause='No complete new periodic save within available observation budget; next read requires '
                          + str(read_budget) + 's, remaining=' + str(round(record.get('stopped_remaining_seconds', 0), 3)) + 's; no save requested')
        else:
            record.update(state='failed-no-replay', cause='First periodic save lacks recovered wait and newer Frame ACK within observation bound')
        runner.persist()
        raise AcceptanceError(record['cause']) from None
    except Exception as error:
        if not record['state'].startswith('inconclusive-'):
            record.update(state='failed-no-replay', cause=f'{type(error).__name__}: {error}')
        runner.persist()
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id',required=True)
    parser.add_argument('--expected-dll-sha256')
    parser.add_argument('--candidate-binary',type=Path)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--verify-client-save-wait',action='store_true')
    parser.add_argument('--wait-seconds',type=float,default=30)
    parser.add_argument('--peer',choices=('both','host','client-1'),default='both')
    args=parser.parse_args(); args.players=2; args.cleanup=False; args.step_timeout=45
    if not args.candidate_binary and (args.verify_client_save_wait or not args.expected_dll_sha256):parser.error('An explicit candidate binary is required for save overlap; retention requires candidate or expected standalone DLL hash')
    if not 1 <= args.wait_seconds <= 120:parser.error('wait-seconds must be between 1 and 120')
    runner=AcceptanceRunner(args); runner.summary['scope']='retain current owned worlds as complete local files, peer='+args.peer+'; no distribution, action replay or fresh regression claim'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=args.expected_run_id: raise AcceptanceError('owned retention run differs')
        if args.verify_client_save_wait:
            result=verify_client_save_wait(runner)
            runner.summary.update(passed=result['confirmed'],state=result['state'],finished_utc=utc())
            runner.persist();print(str(runner.summary_path));return 0 if result['confirmed'] else 2
        peers=('host','client-1') if args.peer=='both' else (args.peer,)
        for peer in peers:
            manifest=read_json(runner.manifests[peer])
            if args.candidate_binary:candidate_binary_receipt(manifest,args.candidate_binary)
            elif manifest_runtime_kind(manifest)==FRAMEWORK_PLUGIN_KIND or manifest['installed_binary_sha256']['MT2_Multiplayer.dll']!=args.expected_dll_sha256: raise AcceptanceError('owned retention DLL differs or explicit Framework candidate absent: '+peer)
            engine=runner.inspect(peer+'-read-before-retention',peer)
            controller=engine.get('typed_actions',{})
            if controller.get('locked') or controller.get('pending'): raise AcceptanceError('native action unresolved before retention: '+peer)
            saved=runner.single(peer+'-local-save-once',peer,'game_capture_local_snapshot')
            saved=verify_local_file(saved,manifest,runner.root/peer,runner.summary['runtime_run_id'])
            if saved.get('state')!='snapshot_file_complete' or saved.get('distribution_requested') is not False: raise AcceptanceError('local-only complete file receipt differs: '+peer)
            runner.checked('native-placed-world-file' if peer=='host' else 'native-client-world-file',saved)
        runner.summary.update(passed=True,state='retained-two-current-worlds')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-world-retention',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining mutations skipped; no Save replay'})
        log('error','world retention: '+str(error))
    runner.summary['finished_utc']=utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1

if __name__=='__main__': raise SystemExit(main())
