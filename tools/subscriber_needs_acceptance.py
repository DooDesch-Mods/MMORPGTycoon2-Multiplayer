"""Observe one selected client's needs fence using real host values and native ticks."""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, utc, log, ready_reason


def replica(engine):
    state = engine.get('subscriber_needs_replica')
    if not isinstance(state, dict) or not state.get('armed') or not state.get('bound') or state.get('error'):
        raise AcceptanceError(f'selected subscriber fence unavailable: {state or engine.get("subscriber_needs_replica_error")}')
    return state


def candidate_ids(value):
    if not isinstance(value, dict):
        raise AcceptanceError('native subscriber candidate receipt is missing')
    for key in ('existing_count', 'scanned_count', 'eligible_count'):
        if type(value.get(key)) is not int or not 0 <= value[key] <= 1000000:
            raise AcceptanceError(f'native candidate counter invalid field={key} actual={value.get(key)}')
    ids = value.get('serialized_ids')
    if not isinstance(ids, list) or len(ids) > 32 or any(type(item) is not int or not 0 <= item <= 2147483647 for item in ids):
        raise AcceptanceError(f'native candidate IDs invalid: {ids}')
    if len(set(ids)) != len(ids) or value['eligible_count'] != len(ids) or not len(ids) <= value['scanned_count'] <= value['existing_count']:
        raise AcceptanceError(f'native candidate accounting failed: {value}')
    if type(value.get('scan_complete')) is not bool or value['scan_complete'] != (value['scanned_count'] == value['existing_count']):
        raise AcceptanceError(f'native candidate completeness differs: {value}')
    return ids


def cycles_reason(before, after):
    for index, name in enumerate(('brain_tick', 'update_needs')):
        old, new = before['ticks'][index], after['ticks'][index]
        entered = new['entered'] - old['entered']
        if new['guard_rejected'] != old['guard_rejected'] or new['forwarded'] - old['forwarded'] != entered:
            raise AcceptanceError(f'selected {name} original forwarding or receiver guard failed')
        if entered < 2:
            return f'waiting for two selected original {name} cycles: actual={entered}'
    suppressed = 0
    for old, new in zip(before['writes'], after['writes']):
        if new['guard_rejected'] != old['guard_rejected'] or new['wrong_thread'] != old['wrong_thread']:
            raise AcceptanceError(f'selected native write guard failed leaf={new["leaf"]}')
        if new['entered'] - old['entered'] != new['forwarded'] - old['forwarded'] + new['suppressed'] - old['suppressed']:
            raise AcceptanceError(f'native write accounting failed leaf={new["leaf"]}')
        suppressed += new['suppressed'] - old['suppressed']
    return None if suppressed else 'waiting for selected native writes to be suppressed'


class NeedsAcceptance(AcceptanceRunner):
    def run(self):
        running = {'host': False, 'client-1': False}
        self.summary['scope'] = 'one subscriber only, real host needs, two original client cycles and native images'
        try:
            self.load_manifests()
            initial = {}
            for peer in running:
                engine = self.inspect(f'{peer}-ready', peer)
                reason = ready_reason(engine, require_placement=False)
                if reason or engine.get('clock', {}).get('scale') != 0:
                    raise AcceptanceError(f'{peer} must be ready and paused: {reason or engine.get("clock")}')
                initial[peer] = engine
            epoch = initial['client-1']['software']['bound_epoch']
            if not epoch:
                raise AcceptanceError('client software epoch is not bound')
            if self.args.serialized_id is not None:
                candidates = [self.args.serialized_id]
            else:
                catalogs = {peer: self.single(f'{peer}-existing-online-ids', peer, 'game_read_subscriber_candidates') for peer in running}
                host_ids, client_ids = candidate_ids(catalogs['host']), set(candidate_ids(catalogs['client-1']))
                candidates = [item for item in host_ids if item in client_ids][:self.args.scan_limit]
                self.checked('native-online-id-candidates', {'receipts': catalogs, 'common_ids': candidates})
            selected = None
            for serialized_id in candidates:
                left = self.single(f'host-needs-{serialized_id}', 'host', 'game_read_subscriber_needs', {'serialized_id': serialized_id})
                if left.get('logged_in') is not True:
                    continue
                right = self.single(f'client-needs-{serialized_id}', 'client-1', 'game_read_subscriber_needs', {'serialized_id': serialized_id})
                if right.get('logged_in') is True and left['needs'] == right['needs']:
                    selected = serialized_id
                    break
            if selected is None:
                raise AcceptanceError(f'no matching logged-in subscriber in diagnostic candidates={list(candidates)}; no arm attempted')
            self.checked('selected-native-subscriber', {'host': left, 'client': right, 'epoch': epoch})
            self.single('client-arm-once', 'client-1', 'game_arm_subscriber_needs',
                        {'epoch': epoch, 'serialized_id': selected, 'expected_initial': right['needs']})
            before = replica(self.inspect('client-armed', 'client-1'))
            self.single('host-resume-source', 'host', 'game_set_time_scale', {'scale': 1})
            running['host'] = True
            deadline, count = time.monotonic() + self.args.observe_seconds, 0
            while True:
                count += 1
                source = self.single(f'host-natural-needs-{count}', 'host', 'game_read_subscriber_needs', {'serialized_id': selected})
                if source['needs'] != left['needs']:
                    break
                if time.monotonic() >= deadline:
                    raise AcceptanceError(f'host needs did not change within{self.args.observe_seconds}s; no fabricated source or replay')
            self.single('host-pause-source', 'host', 'game_set_time_scale', {'scale': 0})
            running['host'] = False
            source = self.single('host-paused-needs', 'host', 'game_read_subscriber_needs', {'serialized_id': selected})
            if source['needs'] == right['needs']:
                raise AcceptanceError('host paused needs equal original client values; observable mutation absent')
            applied = self.single('client-apply-once', 'client-1', 'game_apply_subscriber_needs',
                                  {'epoch': epoch, 'serialized_id': selected, 'revision': 1,
                                   'expected_before': right['needs'], 'host_needs': source['needs']})
            if applied.get('written') is not True or applied.get('completed_setters') != 7 or applied.get('after') != source['needs']:
                raise AcceptanceError(f'native seven-setter receipt failed: {applied}')
            self.checked('real-host-needs-applied', {'host': source, 'receipt': applied})
            self.single('client-resume-cycles', 'client-1', 'game_set_time_scale', {'scale': 1})
            running['client-1'] = True
            deadline, count, previous = time.monotonic() + self.args.observe_seconds, 0, None
            while True:
                count += 1
                state = replica(self.inspect(f'client-native-cycle-{count}', 'client-1'))
                reason = cycles_reason(before, state)
                if reason is None:
                    break
                if reason != previous:
                    log('info', reason)
                    previous = reason
                if time.monotonic() >= deadline:
                    raise AcceptanceError(f'client cycle observation timed out required={self.args.observe_seconds}s condition={reason}; no fake ticks')
            self.single('client-pause-cycles', 'client-1', 'game_set_time_scale', {'scale': 0})
            running['client-1'] = False
            observed = self.single('client-final-needs', 'client-1', 'game_read_subscriber_needs', {'serialized_id': selected})
            if observed['needs'] != source['needs']:
                raise AcceptanceError(f'client selected needs escaped host state: host={source["needs"]} client={observed["needs"]}')
            final = self.inspect('client-final-state', 'client-1')
            self.checked('native-original-cycles-and-fence', {'baseline': before, 'after': replica(final), 'readback': observed})
            for peer in running:
                image = self.single(f'{peer}-native-frame', peer, 'game_capture_frame')
                self.checked(f'{peer}-native-image', image)
            self.single('client-disarm-once', 'client-1', 'game_disarm_subscriber_needs', {'epoch': epoch})
            disarmed = self.inspect('client-disarmed-state', 'client-1').get('subscriber_needs_replica', {})
            if disarmed.get('armed') is not False:
                raise AcceptanceError(f'client fence remained armed after disarm: {disarmed}')
            self.checked('selected-fence-disarmed', disarmed)
            self.summary['passed'] = True
            self.summary['state'] = 'confirmed-selected-subscriber-needs-only'
        except Exception as error:
            self.summary['state'] = 'failed-selected-subscriber-needs'
            self.summary['error'] = {'step': self.summary['step'], 'cause': f'{type(error).__name__}: {error}', 'mutation_replayed': False}
            log('error', f'subscriber needs acceptance failed: {error}')
        finally:
            for peer, resumed in running.items():
                if resumed:
                    try:
                        self.single(f'{peer}-failure-pause', peer, 'game_set_time_scale', {'scale': 0})
                    except Exception as error:
                        self.summary.setdefault('cleanup_errors', []).append({'peer': peer, 'cause': f'{type(error).__name__}: {error}'})
                        log('error', f'failure pause unconfirmed peer={peer} cause={error}; no replay')
            self.summary['finished_utc'] = utc()
            self.persist()
        log('info' if self.summary['passed'] else 'error', f'subscriber needs passed={self.summary["passed"]} summary={self.summary_path}')
        return 0 if self.summary['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root', type=Path, default=ROOT / '_MPTest')
    parser.add_argument('--step-timeout', type=float, default=30)
    parser.add_argument('--observe-seconds', type=float, default=55)
    parser.add_argument('--serialized-id', type=int)
    parser.add_argument('--scan-limit', type=int, default=32)
    args = parser.parse_args()
    if not 0 < args.step_timeout <= 45 or not 0 < args.observe_seconds <= 60 or not 1 <= args.scan_limit <= 128:
        parser.error('diagnostic timeout or scan bound is invalid')
    if args.serialized_id is not None and not 0 <= args.serialized_id <= 2147483647:
        parser.error('serialized-id requires nonnegative int32')
    args.cleanup = False
    return NeedsAcceptance(args).run()


if __name__ == '__main__':
    sys.exit(main())
