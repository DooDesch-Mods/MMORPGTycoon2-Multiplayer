import copy
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / '.github/scripts/source_checks.py'
spec = importlib.util.spec_from_file_location('source_checks', SCRIPT)
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)


class SourceCheckShards(unittest.TestCase):
    def setUp(self):
        self.targets = [
            {'name': 'alpha', 'kind': 'EXECUTABLE', 'path': 'D:/build/alpha.exe'},
            {'name': 'beta', 'kind': 'EXECUTABLE', 'path': 'D:/build/beta.exe'},
            {'name': 'compile_only', 'kind': 'OBJECT_LIBRARY', 'path': '-'},
        ]
        self.tests = [
            {'name': 'native-alpha', 'command': ['d:/BUILD/ALPHA.exe']},
            {'name': 'native-beta', 'command': ['D:/build/beta.exe']},
            {'name': 'python-check', 'command': ['C:/python/python.exe', 'test.py']},
        ]

    def receipts(self):
        targets, tests, catalog, _ = source.test_plan(self.targets, self.tests, 'D:/build', 2)
        return [{'index': i, 'count': 2, 'source_commit': 'abc', 'passed': True, 'targets': targets[i],
                 'tests': tests[i], 'all_targets': sorted(row['name'] for row in self.targets), 'all_tests': catalog}
                for i in range(2)]

    def test_complete_build_and_test_union(self):
        result = source.verify_receipts(self.receipts(), 'abc')
        self.assertEqual((result['targets'], result['tests']), (3, 3))
        self.assertIn('compile_only', sum(source.target_plan(self.targets, 2), []))

    def test_unbuilt_command_is_visible_and_cannot_escape_aggregate(self):
        self.tests.append({'name': 'unbuilt-native'})
        _, _, _, unavailable = source.test_plan(self.targets, self.tests, 'D:/build', 2)
        self.assertEqual(unavailable, ['unbuilt-native'])
        with self.assertRaisesRegex(ValueError, 'CTest coverage'):
            source.verify_receipts(self.receipts(), 'abc')

    def test_unknown_build_executable_rejected(self):
        self.tests.append({'name': 'unknown', 'command': ['D:/build/unknown.exe']})
        with self.assertRaisesRegex(ValueError, 'no CMake target'):
            source.test_plan(self.targets, self.tests, 'D:/build', 2)

    def test_duplicate_target_and_executable_rejected(self):
        with self.assertRaisesRegex(ValueError, 'duplicate CMake'):
            source.target_plan(self.targets + [self.targets[0]], 2)
        duplicate = dict(self.targets[0], name='alias')
        with self.assertRaisesRegex(ValueError, 'duplicate native'):
            source.test_plan(self.targets + [duplicate], self.tests, 'D:/build', 2)

    def test_incomplete_duplicate_or_foreign_receipts_rejected(self):
        good = self.receipts()
        cases = []
        cases.append(good[:1])
        bad = copy.deepcopy(good);bad[1]['index'] = 0;cases.append(bad)
        bad = copy.deepcopy(good);bad[1]['source_commit'] = 'foreign';cases.append(bad)
        bad = copy.deepcopy(good);bad[1]['passed'] = False;cases.append(bad)
        bad = copy.deepcopy(good);bad[1]['all_tests'].append('extra');cases.append(bad)
        bad = copy.deepcopy(good);bad[1]['tests'].extend(bad[0]['tests']);cases.append(bad)
        bad = copy.deepcopy(good);bad[1]['targets'] = [];cases.append(bad)
        for index, rows in enumerate(cases):
            with self.subTest(index=index), self.assertRaises(ValueError):
                source.verify_receipts(rows, 'abc')

    def test_junit_requires_the_actual_selected_checks(self):
        root = source.ET.fromstring('<testsuite><testcase name="alpha"/><testcase name="beta"/></testsuite>')
        self.assertEqual(source.confirmed_test_names(root, ['beta', 'alpha']), ['alpha', 'beta'])
        with self.assertRaisesRegex(ValueError, 'executed CTest'):
            source.confirmed_test_names(root, ['alpha'])
        for tag in ('failure', 'error', 'skipped'):
            bad = source.ET.fromstring('<testsuite><testcase name="alpha"><' + tag + '/></testcase></testsuite>')
            with self.subTest(tag=tag), self.assertRaisesRegex(ValueError, 'failed or skipped'):
                source.confirmed_test_names(bad, ['alpha'])

    def test_empty_catalog_cannot_pass(self):
        with self.assertRaisesRegex(ValueError, 'catalog is empty'):
            source.test_plan(self.targets, [], 'D:/build', 2)
        rows = self.receipts()
        for row in rows:
            row['all_tests'] = [];row['tests'] = []
        with self.assertRaisesRegex(ValueError, 'catalog is empty'):
            source.verify_receipts(rows, 'abc')

    def test_name_and_count_guards(self):
        with self.assertRaises(ValueError):
            source.target_plan(self.targets, 0)
        with self.assertRaisesRegex(ValueError, 'duplicate CTest'):
            source.test_plan(self.targets, self.tests + [self.tests[0]], 'D:/build', 2)
        with self.assertRaisesRegex(ValueError, 'line break'):
            source.test_plan(self.targets, [{'name': 'bad' + chr(10), 'command': ['python']}], 'D:/build', 2)


if __name__ == '__main__':
    unittest.main()
