import copy
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import textwrap
import unittest
from unittest.mock import mock_open, patch

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

    def test_fixture_provider_and_consumer_keep_the_native_owner(self):
        label = {'name': 'LABELS', 'value': ['source-target:alpha']}
        provider = {'name': 'provider', 'properties': [label, {'name': 'FIXTURES_SETUP', 'value': ['png']}]}
        consumer = {'name': 'consumer', 'command': ['C:/python/python.exe'],
                    'properties': [label, {'name': 'FIXTURES_REQUIRED', 'value': ['png']}]}
        groups, tests, _, unavailable = source.test_plan(self.targets, [provider, consumer], 'D:/build', 2)
        owner = next(i for i, group in enumerate(groups) if 'alpha' in group)
        self.assertEqual(tests[owner], ['consumer', 'provider'])
        self.assertEqual(unavailable, ['provider'])

    def test_unassigned_or_unknown_fixture_owner_rejected(self):
        fixture = {'name': 'consumer', 'command': ['python'],
                   'properties': [{'name': 'FIXTURES_REQUIRED', 'value': ['png']}]}
        with self.assertRaisesRegex(ValueError, 'explicit source-target'):
            source.test_plan(self.targets, [fixture], 'D:/build', 2)
        fixture['properties'].append({'name': 'LABELS', 'value': ['source-target:unknown']})
        with self.assertRaisesRegex(ValueError, 'owner is missing'):
            source.test_plan(self.targets, [fixture], 'D:/build', 2)

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


class WindowsWorkflowRouting(unittest.TestCase):
    workflow = (SCRIPT.parents[1] / 'workflows/windows.yml').read_text(encoding='utf-8')
    classifier = textwrap.dedent(workflow.split("          python3 - <<'PY'\n", 1)[1].split('          PY\n', 1)[0])

    def route(self, paths=(), candidate=True, before='a' * 40, failure=None):
        environment = {'ROUTE_CANDIDATE': str(candidate).lower(), 'PUSH_BEFORE': before,
                       'GITHUB_SHA': 'b' * 40, 'GITHUB_OUTPUT': 'route-output'}
        with patch.dict(os.environ, environment), contextlib.redirect_stdout(io.StringIO()), \
                patch('subprocess.run', side_effect=failure) as fetch, \
                patch('subprocess.check_output', return_value=b'\0'.join(paths) + b'\0') as diff, \
                patch('builtins.open', mock_open()) as output:
            if failure:
                with self.assertRaises(subprocess.CalledProcessError):
                    exec(self.classifier, {})
                output.assert_not_called()
                return
            exec(self.classifier, {})
            if candidate and before != '0' * 40:
                fetch.assert_called_once_with(['git', 'fetch', '--no-tags', '--depth=1', 'origin', before], check=True)
                diff.assert_called_once_with(['git', 'diff', '--no-renames', '--name-only', '-z', before, 'b' * 40])
            else:
                fetch.assert_not_called()
                diff.assert_not_called()
            output.assert_called_once_with('route-output', 'a', encoding='utf-8')
            return output().write.call_args.args[0].strip()

    def test_only_known_milestone_pushes_delegate(self):
        marker = b'.github/beta-milestone.json'
        self.assertEqual(self.route([marker, b'src/main.cpp']), 'run_checks=false')
        self.assertEqual(self.route([b'src/main.cpp']), 'run_checks=true')
        self.assertEqual(self.route([]), 'run_checks=true')
        self.assertEqual(self.route([marker], candidate=False), 'run_checks=true')
        self.assertEqual(self.route([marker], before='0' * 40), 'run_checks=true')

    def test_github_path_filter_boundary_keeps_uncertain_pushes(self):
        paths = [b'.github/beta-milestone.json'] + [str(i).encode() for i in range(299)]
        self.assertEqual(self.route(paths), 'run_checks=false')
        self.assertEqual(self.route(paths + [b'overflow']), 'run_checks=true')

    def test_failed_range_fetch_cannot_report_a_successful_route(self):
        self.route(failure=subprocess.CalledProcessError(128, ['git', 'fetch']))

    def test_event_guards_and_aggregate_preserve_required_checks(self):
        self.assertIn("github.event_name == 'push' && github.ref == 'refs/heads/main'", self.workflow)
        self.assertIn("github.workflow == 'Windows source checks'", self.workflow)
        self.assertIn("github.repository == 'DooDesch-Mods/MMORPGTycoon2-Multiplayer' && inputs.beta_release != true", self.workflow)
        self.assertEqual(self.workflow.count("if: needs.route.outputs.run_checks == 'true'"), 3)
        self.assertIn("always() && (needs.route.result != 'success' || needs.route.outputs.run_checks != 'false')", self.workflow)
        self.assertIn('needs: [route, source-tests, python-tests, compile]', self.workflow)
        beta = (SCRIPT.parents[1] / 'workflows/beta.yml').read_text(encoding='utf-8')
        self.assertIn('uses: ./.github/workflows/windows.yml\n    with:\n      beta_release: true', beta)


if __name__ == '__main__':
    unittest.main()
