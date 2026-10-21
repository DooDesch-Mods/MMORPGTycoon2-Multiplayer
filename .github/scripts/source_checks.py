"""Build disjoint source-check targets and verify complete CTest coverage."""
import argparse
import json
import ntpath
from pathlib import Path
import subprocess
import sys
import traceback
import zlib
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]


def path_key(value):
    return ntpath.normcase(ntpath.normpath(str(value)))


def inside(path, directory):
    try:
        return ntpath.commonpath([path_key(path), path_key(directory)]) == path_key(directory)
    except ValueError:
        return False  # Different Windows drives cannot contain each other.


def target_plan(targets, count):
    if count < 1:
        raise ValueError('shard count must be positive')
    names = [row['name'] for row in targets]
    if len(names) != len(set(names)):
        raise ValueError('duplicate CMake build target')
    return [sorted(names)[index::count] for index in range(count)]


def test_plan(targets, tests, build, count):
    groups = target_plan(targets, count)
    owners = {name: index for index, group in enumerate(groups) for name in group}
    executables = {}
    for target in targets:
        if target['kind'] != 'EXECUTABLE':
            continue
        key = path_key(target['path'])
        if key in executables:
            raise ValueError('duplicate native executable path: ' + key)
        executables[key] = owners[target['name']]
    names = [test['name'] for test in tests]
    if not names:
        raise ValueError('CTest catalog is empty')
    if len(names) != len(set(names)):
        raise ValueError('duplicate CTest name')
    selected = [[] for _ in range(count)]
    unavailable = []
    for test in tests:
        name = test['name']
        if chr(10) in name or chr(13) in name:
            raise ValueError('CTest name contains a line break')
        properties = {row['name']: row['value'] for row in test.get('properties', [])}
        labels = properties.get('LABELS', [])
        if isinstance(labels, str):
            labels = labels.split(';')
        forced = [label.removeprefix('source-target:') for label in labels if label.startswith('source-target:')]
        fixture = any(properties.get(key) for key in ('FIXTURES_SETUP', 'FIXTURES_REQUIRED', 'FIXTURES_CLEANUP', 'DEPENDS'))
        if fixture and not forced:
            raise ValueError('dependent CTest needs an explicit source-target owner: ' + name)
        if forced:
            if len(forced) != 1 or forced[0] not in owners:
                raise ValueError('CTest source-target owner is missing or ambiguous: ' + name)
            selected[owners[forced[0]]].append(name)
            if not test.get('command'):
                unavailable.append(name)
            continue
        command = test.get('command')
        if not command:
            # CTest omits commands for binaries not built on this runner.
            # The aggregate requires every registered test in one successful receipt.
            unavailable.append(name)
            continue
        key = path_key(command[0])
        if key in executables:
            owner = executables[key]
        elif inside(key, build):
            raise ValueError('CTest executable has no CMake target: ' + command[0])
        else:
            owner = zlib.crc32(name.encode('utf-8')) % count
        selected[owner].append(name)
    return groups, [sorted(group) for group in selected], sorted(names), sorted(unavailable)


def verify_receipts(receipts, commit):
    if not receipts:
        raise ValueError('no source-check receipts')
    count = receipts[0]['count']
    if count < 1 or len(receipts) != count:
        raise ValueError('source-check shard count incomplete')
    if sorted(row['index'] for row in receipts) != list(range(count)):
        raise ValueError('source-check shard indices missing or duplicated')
    expected_targets = receipts[0]['all_targets']
    expected_tests = receipts[0]['all_tests']
    if not expected_targets or not expected_tests:
        raise ValueError('source-check catalog is empty')
    executed_targets, executed_tests = [], []
    for row in receipts:
        if row.get('passed') is not True or row['source_commit'] != commit:
            raise ValueError('source-check execution/source identity not confirmed')
        if row['count'] != count or row['all_targets'] != expected_targets or row['all_tests'] != expected_tests:
            raise ValueError('source-check catalog differs between shards')
        if row['targets'] != sorted(expected_targets)[row['index']::count]:
            raise ValueError('source-check target partition differs')
        executed_targets.extend(row['targets'])
        executed_tests.extend(row['tests'])
    if len(executed_targets) != len(set(executed_targets)) or sorted(executed_targets) != sorted(expected_targets):
        raise ValueError('source-check build coverage is missing or duplicated')
    if len(executed_tests) != len(set(executed_tests)) or sorted(executed_tests) != sorted(expected_tests):
        raise ValueError('CTest coverage is missing or duplicated')
    return {'source_commit': commit, 'shards': count, 'targets': len(executed_targets), 'tests': len(executed_tests), 'passed': True}



def confirmed_test_names(root, expected):
    cases = root.findall('.//testcase')
    names = [case.attrib['name'] for case in cases]
    if len(names) != len(set(names)) or sorted(names) != sorted(expected):
        raise ValueError('executed CTest names differ from selected shard')
    if any(case.find(tag) is not None for case in cases for tag in ('failure', 'error', 'skipped')):
        raise ValueError('CTest shard contains failed or skipped checks')
    return sorted(names)


def read_targets(build, config):
    rows = []
    for line in (build / ('source-check-targets-' + config + '.tsv')).read_text(encoding='utf-8').splitlines():
        if not line:
            continue
        name, kind, path = line.split(chr(9))
        rows.append({'name': name, 'kind': kind, 'path': path})
    if not rows:
        raise ValueError('CMake source-check target map is empty')
    return rows


def catalog(build, config):
    command = ['ctest', '--test-dir', str(build), '-C', config, '--show-only=json-v1']
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    return json.loads(result.stdout)['tests']


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + chr(10), encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['plan', 'run', 'verify'])
    parser.add_argument('--build', type=Path)
    parser.add_argument('--config', default='Release')
    parser.add_argument('--count', type=int, default=8)
    parser.add_argument('--index', type=int, default=0)
    parser.add_argument('--parallel', type=int, default=4)
    parser.add_argument('--receipts', type=Path)
    parser.add_argument('--commit')
    args = parser.parse_args()
    if args.mode == 'verify':
        if not args.receipts or not args.commit:
            parser.error('verify requires --receipts and --commit')
        rows = [json.loads(path.read_text(encoding='utf-8')) for path in sorted(args.receipts.glob('source-shard-*.json'))]
        result = verify_receipts(rows, args.commit)
        save(args.receipts / 'source-check-coverage.json', result)
        print(json.dumps(result))
        return
    if not args.build or not 0 <= args.index < args.count or args.parallel < 1:
        parser.error('valid --build, --index, --count and --parallel required')
    build = args.build.resolve()
    targets = read_targets(build, args.config)
    groups = target_plan(targets, args.count)
    if args.mode == 'plan':
        _, tests, all_tests, unavailable = test_plan(targets, catalog(build, args.config), build, args.count)
        result = {'count': args.count, 'all_targets': sorted(row['name'] for row in targets), 'all_tests': all_tests,
                  'targets': groups, 'tests': tests, 'unavailable_tests': unavailable, 'execution': 'planned'}
        save(build / 'source-check-plan.json', result)
        print(json.dumps({'targets': len(targets), 'tests': len(all_tests), 'unavailable': len(unavailable),
                          'shards': [{'index': i, 'targets': len(groups[i]), 'tests': len(tests[i])} for i in range(args.count)]}))
        return
    commit = subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip()
    if args.commit and args.commit != commit:
        raise ValueError('source-check checkout differs from requested commit')
    receipt = {'index': args.index, 'count': args.count, 'source_commit': commit, 'targets': groups[args.index],
               'all_targets': sorted(row['name'] for row in targets), 'tests': [], 'all_tests': [], 'passed': False}
    output = build / ('source-shard-' + str(args.index) + '.json')
    try:
        if receipt['targets']:
            command = ['cmake', '--build', str(build), '--config', args.config, '--parallel', str(args.parallel), '--target', *receipt['targets']]
            print('info: building source-check shard ' + str(args.index) + ' targets=' + str(len(receipt['targets'])), flush=True)
            subprocess.run(command, check=True)
        _, tests, all_tests, unavailable = test_plan(targets, catalog(build, args.config), build, args.count)
        receipt.update(tests=tests[args.index], all_tests=all_tests, unavailable_tests=unavailable)
        if receipt['tests']:
            selected = build / ('source-shard-' + str(args.index) + '-tests.txt')
            selected.write_text(chr(10).join(receipt['tests']) + chr(10), encoding='utf-8')
            subprocess.run(['ctest', '--test-dir', str(build), '-C', args.config, '--tests-from-file', str(selected),
                            '--no-tests=error', '--output-on-failure', '--timeout', '60', '--output-junit',
                            str(build / ('source-shard-' + str(args.index) + '.xml'))], check=True)
            receipt['tests'] = confirmed_test_names(ET.parse(build / ('source-shard-' + str(args.index) + '.xml')).getroot(), receipt['tests'])
        else:
            print('info: source-check shard contains build-only targets', flush=True)
        receipt['passed'] = True
    except Exception as error:
        receipt['cause'] = str(error)
        raise
    finally:
        save(output, receipt)
    print(json.dumps({'index': args.index, 'targets': len(receipt['targets']), 'tests': len(receipt['tests']), 'passed': True}))


if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('error: source checks: ' + str(error), file=sys.stderr)
        traceback.print_exc()
        sys.exit(1)
