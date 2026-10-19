"""Probe time items from an exact existing paused host without replaying its load."""
import argparse
import sys
import uuid

from large_save_acceptance import LargeSaveRunner, parser as large_parser
from local_acceptance import AcceptanceError, log, read_json, utc, validate_snapshot


class InventoryRunner(LargeSaveRunner):
    def run_existing(self, expected_run):
        self.summary['scope'] = 'one fresh capture from an existing paused owned large world and read-only client time items'
        self.summary['catalog'] = {'state':'deliberately-skipped', 'cause':'retained world_root_shard_invalid guard requires a changed native candidate'}
        self.persist()
        try:
            self.checked('imported-copy-before', self.input_snapshot())
            self.load_manifests()
            for peer in ('host','client-1'):
                manifest = read_json(self.manifests[peer])
                if manifest['run_id'] != expected_run:
                    raise AcceptanceError(f'Existing run changed peer={peer} actual={manifest["run_id"]} required={expected_run}; capture not attempted')
                pending = self.single(f'preflight-pending-{peer}',peer,'observe_pending')
                if pending.get('pending') is not False or 'response' in pending:
                    raise AcceptanceError(f'Unresolved native request peer={peer}: {pending}; no mutation attempted')
            self.paused_engine('host','existing-host-paused')
            captured = self.once('host-capture-once','host','game_capture_snapshot')
            captured_id = str(uuid.UUID(captured['snapshot_id']))
            captured = validate_snapshot(captured,captured_id,self.root/'host')
            self.checked('host-native-capture',captured)
            self.summary['proof']['host_native_capture'] = True
            self.transfer(captured)
            self.summary['proof']['transfer_and_client_load'] = True
            self.paused_engine('client-1','client-native-load-readback')
            state = self.single('client-native-time-items','client-1','game_read_time_ui')
            if state.get('capture_enabled') is not False or state.get('host_state_available') is not False:
                raise AcceptanceError(f'Unexpected time capture or presentation store: {state}')
            items = state.get('items')
            if not isinstance(items,list) or [item.get('scale') for item in items] != [0,1,3,9]:
                raise AcceptanceError(f'Native time-item scales differ: {items}')
            self.checked('client-native-time-items',state)
            self.summary['proof']['native_time_items'] = True
            self.summary['state'],self.summary['passed'] = 'confirmed-bounded-native-time-read',True
        except Exception as error:
            self.summary['state'],self.summary['passed'] = 'failed',False
            self.summary['error'] = {'step':self.summary['step'],'cause':f'{type(error).__name__}: {error}','mutation_replayed':False}
            log('error',f'Native inventory acceptance failed step={self.summary["step"]} cause={error}')
        finally:
            try:
                self.checked('imported-copy-preserved',self.input_snapshot())
            except Exception as error:
                self.summary['state'],self.summary['passed'] = 'failed',False
                self.summary['preservation_error'] = f'{type(error).__name__}: {error}'
                log('error',f'Imported-copy preservation failed cause={error}')
            self.summary['finished_utc'] = utc()
            self.persist()
        log('info' if self.summary['passed'] else 'error',f'Native inventory passed={self.summary["passed"]} summary={self.summary_path}')
        return 0 if self.summary['passed'] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run',type=lambda value:str(uuid.UUID(value)),required=True)
    values = parser.parse_args()
    args = large_parser().parse_args([])
    args.cleanup = False
    args.snapshot_id = '106937b1-b5c8-4348-8939-b526cef4ea04'
    return InventoryRunner(args).run_existing(values.expected_run)


if __name__ == '__main__':
    sys.exit(main())
