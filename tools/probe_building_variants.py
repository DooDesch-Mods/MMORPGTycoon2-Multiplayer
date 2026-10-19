"""Probe real unlocked package variants without creating native objects."""
import argparse
from local_acceptance import AcceptanceRunner, AcceptanceError, ROOT, read_json, utc, log
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--expected-run-id',required=True)
    args=parser.parse_args(); args.test_root=ROOT/'_MPTest'; args.players=2; args.cleanup=False; args.step_timeout=45
    runner=AcceptanceRunner(args); runner.summary['scope']='actual native catalog/lock/ground probes for new families; no building creation or unlock change'
    try:
        runner.load_manifests()
        if runner.summary['runtime_run_id']!=args.expected_run_id: raise AcceptanceError('variant probe owned run differs')
        for kind,variant in (('SmallInn','corrupt'),('Inn','boat'),('Blacksmith','elf')):
            result=runner.single(kind+'-'+variant+'-readonly-ground','host','game_probe_building_placement',{'type':kind,'variant':variant,'x':8200,'z':8550})
            runner.checked('native-variant-ground',{'type':kind,'variant':variant,'ground':result})
        runner.summary.update(passed=True,state='confirmed-native-variant-ground')
    except Exception as error:
        runner.summary.update(passed=False,state='failed-native-variant-ground',error={'step':runner.summary['step'],'cause':str(error),'behavior':'remaining probes skipped; no unlock change or native creation'})
        log('error','native variant probe: '+str(error))
    runner.summary['finished_utc']=utc(); runner.persist(); print(str(runner.summary_path))
    return 0 if runner.summary['passed'] else 1
if __name__=='__main__': raise SystemExit(main())
