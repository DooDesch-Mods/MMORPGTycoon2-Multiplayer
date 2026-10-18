"""Durable completion checks for the original Abort-before-JobCompleted path."""
import ast
import copy
import importlib.util
from pathlib import Path
import unittest

TREE = Path(__file__).resolve().parents[2]
SOURCE = TREE / 'tools/native_bug_completion_readback.py'
spec = importlib.util.spec_from_file_location('completion_readback', SOURCE)
driver = importlib.util.module_from_spec(spec)
spec.loader.exec_module(driver)


class Tests(unittest.TestCase):
    def test_sample_requires_schema_and_component_for_current_vip_arming(self):
        class Runner:
            def __init__(self, armed, schema, component):
                frame = {'schema': schema}
                if component is not None:
                    frame['vips'] = component
                self.engine = {'clock': {'scale': 0, 'paused': True}, 'typed_actions': {
                    'initialized': True, 'started': True, 'locked': False, 'pending': False,
                    'failed': 0, 'vip_frame': {'armed': armed},
                    'live_state': {'active': True, 'locked': False, 'confirmed': True},
                    'native_economy': {'day': {'frame': frame}}}}

            def inspect(self, *_):
                return self.engine

            def single(self, *_):
                return {'observed': True, 'developer_movement': {'observed': True},
                        'jobs': {'queue_count': 0, 'jobs': []}}

        for armed, schema, component, accepted in (
                (False, 12, None, True), (True, 13, {}, True),
                (False, 13, {}, False), (True, 12, None, False),
                (True, 13, None, False), (True, 13, [], False),
                (False, 12, {}, False)):
            with self.subTest(armed=armed, schema=schema, component=component):
                runner = Runner(armed, schema, component)
                if accepted:
                    driver.sample(runner, 'client-1', 'schema', require_bug_work=False)
                else:
                    with self.assertRaises(driver.AcceptanceError):
                        driver.sample(runner, 'client-1', 'schema', require_bug_work=False)

    def values(self):
        selected = {'bug': [0, 'parent', 0, 'snapshot', 0, 0], 'job': [0, 'snapshot', 0, 34], 'fixed': [0, 0, 0, 1, 0]}
        frame = {'bugs': [], 'developers': {'construction': {'bug_jobs': [], 'bug_parents': [[0, 'parent', 2]], 'job_fixed': selected['fixed'][:]}}}
        native = {'jobs': {'queue_count': 0, 'jobs': [], 'developers': [{'job_present': False, 'reciprocal': False}]},
                  'developers': [{'id': i, 'state': 0, 'job_link': False} for i in range(50)], 'ledger': {'cash': 87400}}
        final = {p: {'frame': copy.deepcopy(frame), 'native': copy.deepcopy(native)} for p in driver.PEERS}
        uis = {p: {'observed': True, 'locked': False, 'known_bugs': []} for p in driver.PEERS}
        return final, uis, selected

    def test_completion_with_unchanged_actual_job_fixed_counter(self):
        final, uis, selected = self.values()
        result = driver.validate(final, uis, selected)
        self.assertEqual(result['job_fixed_before'], result['job_fixed_after'])
        self.assertTrue(result['selected_retired'])

    def test_selective_retirement_and_physical_effects_required(self):
        for defect in ('bug', 'job', 'parent', 'queue', 'edge', 'developer', 'tracker', 'ledger'):
            with self.subTest(defect=defect):
                final, uis, selected = self.values()
                for v in final.values():
                    w = driver.work(v)
                    if defect == 'bug': v['frame']['bugs'] = [selected['bug']]
                    if defect == 'job': w['bug_jobs'] = [selected['bug'] + selected['job'] + [7, True, 0]]
                    if defect == 'parent': w['bug_parents'][0][2] = 1
                    if defect == 'queue': v['native']['jobs']['queue_count'] = 1
                    if defect == 'edge': v['native']['jobs']['developers'][0]['job_present'] = True
                    if defect == 'developer': v['native']['developers'][0]['job_link'] = True
                if defect == 'tracker': uis['host']['known_bugs'] = [{'bug': selected['bug']}]
                if defect == 'ledger': final['host']['native']['ledger']['cash'] += 1
                with self.assertRaises(driver.AcceptanceError): driver.validate(final, uis, selected)

    def test_entrypoint_has_only_existing_read_operations(self):
        tree = ast.parse(SOURCE.read_text(encoding='utf-8'))
        singles = [node for node in ast.walk(tree) if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) and node.func.attr == 'single']
        self.assertEqual({node.args[2].value for node in singles}, {'game_read_native_developers', 'game_read_native_bug_fix_ui'})
        self.assertNotIn('set_clock_once', SOURCE.read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
