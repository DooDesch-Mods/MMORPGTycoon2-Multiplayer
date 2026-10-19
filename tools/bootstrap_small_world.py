"""Create and transfer one fresh small owned test world through the game MCP."""
import argparse
from pathlib import Path
import sys
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log, ledger_state, compare_ledgers, validate_snapshot

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--test-root',type=Path,default=ROOT/'_MPTest')
    parser.add_argument('--step-timeout',type=float,default=45)
    parser.add_argument('--ready-timeout',type=float,default=180)
    args=parser.parse_args();args.cleanup=False;args.restart=False
    runner=AcceptanceRunner(args)
    runner.summary['scope']='fresh small world creation, native save, client load and initial paused readback'
    try:
        runner.command('stop-small-owned',[sys.executable,'-X','utf8',str(ROOT/'tools/stop_local_test.py'),'--test-root',str(runner.root)])
        runner.command('prepare-small-profile',[sys.executable,'-X','utf8',str(ROOT/'tools/prepare_small_world_profile.py'),'--test-root',str(runner.root)])
        runner.command('start-small-owned',[sys.executable,'-X','utf8',str(ROOT/'tools/local_test.py'),'--test-root',str(runner.root),'--ready-timeout',str(args.ready_timeout)])
        runner.load_manifests()
        for peer in ('host','client-1'):runner.poll_title(peer)
        created=runner.single('host-create-once','host','game_create_test_world')
        runner.checked('fresh-native-world-created',created)
        runner.single('host-pause-once','host','game_set_time_scale',{'scale':0})
        host=runner.inspect('host-created-world','host')
        runner.checked('host-initial-paused-ledger',ledger_state(host))
        captured=runner.single('host-capture-once','host','game_capture_snapshot')
        runner.checked('small-initial-snapshot',validate_snapshot(captured,captured['snapshot_id'],runner.root/'host'))
        client=runner.poll_client(captured['snapshot_id'])
        host=runner.inspect('host-client-world-ready','host')
        runner.checked('small-host-client-ledger',compare_ledgers(host,client))
        for peer in ('host','client-1'):runner.capture(peer)
        runner.summary.update(state='confirmed-small-world-bootstrap',passed=True,snapshot_id=captured['snapshot_id'],
                              snapshot_bytes=captured['bytes'],tutorial_visibility='requires native frame inspection')
    except Exception as error:
        runner.summary.update(state='failed-small-world-bootstrap',passed=False,error={'step':runner.summary['step'],'cause':f'{type(error).__name__}: {error}','mutation_replayed':False})
        log('error',f'fresh small world failed: {error}; remaining mutations skipped')
    runner.summary['finished_utc']=utc();runner.persist()
    log('info' if runner.summary['passed'] else 'error',f'fresh small world passed={runner.summary["passed"]} summary={runner.summary_path}')
    return 0 if runner.summary['passed'] else 1

if __name__=='__main__':sys.exit(main())
