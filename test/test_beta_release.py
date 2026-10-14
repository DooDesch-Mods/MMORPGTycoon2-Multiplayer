"""Reject incomplete beta packages and unsafe release acknowledgements."""
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / '.github/scripts'
sys.path.insert(0, str(SCRIPTS))
import build_beta
import publish_beta

TAG = 'v0.1.0-beta.1'
COMMIT = 'a' * 40
CANDIDATE = TAG + '@' + COMMIT


def package(path, edit=None):
    entries = {name: b'fixture' for name in build_beta.NOTICES | build_beta.PAYLOAD
               | {'_internal/python313.dll', '_internal/base_library.zip'}}
    if edit:
        edit(entries)
    manifest = {'candidate': CANDIDATE, 'version': TAG[1:], 'source_commit': COMMIT,
                'manual_test_package': True, 'ready_for_manual_testing': True, 'online_acceptance_passed': False,
                'files': {name: {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
                          for name, data in entries.items()}}
    entries['package.json'] = json.dumps(manifest).encode()
    with zipfile.ZipFile(path, 'w') as zipped:
        for name, data in entries.items():
            zipped.writestr(name, data)
    path.with_suffix('.zip.sha256').write_text(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n')


class ReleaseAPI:
    def __init__(self, directory, existing=False):
        self.private = True
        self.commit = COMMIT
        self.writes = []
        self.bad_ack = False
        self.release = {'id': 1, 'tag_name': TAG, 'target_commitish': COMMIT, 'prerelease': True,
                        'draft': not existing, 'html_url': 'https://github.com/private/beta', 'assets': [],
                        'upload_url': 'https://uploads.github.com/repos/' + publish_beta.REPOSITORY + '/releases/1/assets{?name,label}'}
        self.exists = existing
        if existing:
            self.release['assets'] = [self.asset(p.name, p.read_bytes()) for p in directory.iterdir()]

    @staticmethod
    def asset(name, data):
        return {'name': name, 'size': len(data), 'digest': 'sha256:' + hashlib.sha256(data).hexdigest(), 'state': 'uploaded'}

    def __call__(self, method, url, payload=None, content_type=None, missing=False):
        if method != 'GET':
            self.writes.append((method, url))
        if '/commits/' in url:
            return {'sha': self.commit}
        if '/releases/tags/' in url:
            return copy.deepcopy(self.release) if self.exists else None
        if method == 'POST' and '/releases/1/assets?' in url:
            name = url.split('?name=', 1)[1]
            asset = self.asset(name, payload)
            self.release['assets'].append(asset)
            return dict(asset, digest='wrong') if self.bad_ack else asset
        if url.endswith('/assets'):
            return copy.deepcopy(self.release['assets'])
        if method == 'POST' and url.endswith('/releases'):
            self.exists = True
            return copy.deepcopy(self.release)
        if method == 'PATCH':
            self.release.update(payload)
        if '/releases/1' in url:
            return copy.deepcopy(self.release)
        return {'private': self.private}


class BetaReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.archive = self.directory / ('MMORPGTycoon2-Multiplayer-' + TAG + '-windows-x64.zip')
        package(self.archive)

    def test_complete_tagged_package(self):
        self.assertEqual(build_beta.validate(self.archive, CANDIDATE)['version'], TAG[1:])

    def test_required_payload_cannot_be_omitted(self):
        for name in ['LICENSE.md', 'licenses/SDL3.txt', 'MT2MP-Start.exe', '_internal/python313.dll', 'mt2mp_ui/browser.win']:
            with self.subTest(name=name):
                package(self.archive, lambda entries: entries.pop(name))
                with self.assertRaisesRegex(ValueError, 'missing'):
                    build_beta.validate(self.archive, CANDIDATE)

    def test_game_private_and_traversal_files_rejected(self):
        for name in ['MT2.exe', '_internal/MT2.exe', '_internal/save.vrz', '../secret.txt', 'data/settings.json']:
            with self.subTest(name=name):
                package(self.archive, lambda entries: entries.update({name: b'private'}))
                with self.assertRaises(ValueError):
                    build_beta.validate(self.archive, CANDIDATE)

    def test_wrong_candidate_rejected(self):
        with self.assertRaisesRegex(ValueError, 'candidate'):
            build_beta.validate(self.archive, TAG + '@' + 'b' * 40)

    def test_changed_payload_cannot_use_old_manifest(self):
        with zipfile.ZipFile(self.archive) as zipped:
            entries = {n: zipped.read(n) for n in zipped.namelist()}
        entries['MT2_Multiplayer.dll'] = b'different build'
        with zipfile.ZipFile(self.archive, 'w') as zipped:
            for name, data in entries.items():
                zipped.writestr(name, data)
        with self.assertRaisesRegex(ValueError, 'content differs'):
            build_beta.validate(self.archive, CANDIDATE)

    def test_cmake_beta_version_is_one_subprocess_argument(self):
        (self.directory / 'CMakeLists.txt').write_text('project(MT2_Multiplayer VERSION 0.1.0 LANGUAGES CXX)')
        helper = self.directory / '.github/scripts/build_beta.py'
        with patch.object(build_beta, '__file__', str(helper)), patch.object(sys, 'argv', [str(helper), '--tag', TAG, '--commit', COMMIT, '--build-only']), patch.object(build_beta.subprocess, 'run') as run:
            build_beta.main()
        configure = run.call_args_list[0].args[0]
        self.assertIn('-DMT2MP_BUILD_VERSION=0.1.0-beta.1', configure)
        self.assertEqual(len(run.call_args_list), 2)
        self.assertTrue(all(call.kwargs['check'] for call in run.call_args_list))

    def test_false_manifest_or_online_claim_rejected(self):
        for field, value in [('version', '0.1.0'), ('source_commit', 'b' * 40), ('ready_for_manual_testing', False), ('online_acceptance_passed', True), ('files', {})]:
            with self.subTest(field=field):
                package(self.archive)
                with zipfile.ZipFile(self.archive) as zipped:
                    entries = {n: zipped.read(n) for n in zipped.namelist()}
                manifest = json.loads(entries['package.json'])
                manifest[field] = value
                entries['package.json'] = json.dumps(manifest).encode()
                with zipfile.ZipFile(self.archive, 'w') as zipped:
                    for name, data in entries.items():
                        zipped.writestr(name, data)
                with self.assertRaises(ValueError):
                    build_beta.validate(self.archive, CANDIDATE)

    def test_canonical_package_builder_contract(self):
        source_root = next(path for path in [ROOT, *ROOT.parents] if (path / 'tools/package.py').is_file())
        spec = importlib.util.spec_from_file_location('canonical_beta_package', source_root / 'tools/package.py')
        actual = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(actual)
        fixture = self.directory / 'source'
        build = fixture / 'build'
        launcher = fixture / 'launcher/MT2MP-Start.exe'
        files = {'CMakeLists.txt': b'project(MT2_Multiplayer VERSION 0.1.0 LANGUAGES CXX)',
                 'LICENSE.md': b'project notice', 'THIRD_PARTY_NOTICES.md': b'vendor notice',
                 'tools/packaging/FRIEND_TEST.txt': b'tester instructions',
                 'include/vendor/minhook/LICENSE.txt': b'MinHook', 'include/vendor/imgui/LICENSE.txt': b'ImGui',
                 'include/vendor/nlohmann/LICENSE.MIT': b'json', 'include/vendor/SDL3/LICENSE.txt': b'SDL',
                 'tools/packaging/licenses/Python.txt': b'Python', 'tools/packaging/licenses/PyInstaller.txt': b'PyInstaller',
                 'launcher/MT2MP-Start.exe': b'MZfixture', 'launcher/_internal/python313.dll': b'MZpython',
                 'launcher/_internal/base_library.zip': b'python library'}
        files.update({'build/' + name: b'MZfixture' for name in [*actual.BINARY_NAMES, 'mt2mp_sidecar.exe']})
        files.update({'mt2mp_ui/' + name: b'owned UI fixture' for name in actual.UI_NAMES})
        for name, data in files.items():
            path = fixture / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        pin = fixture / 'freeze.json'
        pin.write_text(json.dumps({'candidate': CANDIDATE, 'source_commit': COMMIT,
                                  'dll_sha256': hashlib.sha256((build / 'MT2_Multiplayer.dll').read_bytes()).hexdigest()}))
        entries, manifest = actual.collect(fixture, build, pin, True, launcher, TAG[1:])
        archive = self.directory / 'canonical.zip'
        actual.write_package(entries, archive)
        self.assertEqual(build_beta.validate(archive, CANDIDATE)['source_commit'], COMMIT)

    def test_new_release_publishes_after_both_uploads(self):
        service = ReleaseAPI(self.directory)
        with patch.object(publish_beta, 'api', service):
            publish_beta.publish(TAG, COMMIT, self.directory)
        self.assertEqual([method for method, _ in service.writes], ['POST', 'POST', 'POST', 'PATCH'])
        self.assertFalse(service.release['draft'])

    def test_identical_publish_does_not_write_again(self):
        service = ReleaseAPI(self.directory, existing=True)
        with patch.object(publish_beta, 'api', service):
            publish_beta.publish(TAG, COMMIT, self.directory)
        self.assertEqual(service.writes, [])

    def test_guard_failures_do_not_publish(self):
        for change, cause in [
            (lambda s: setattr(s, 'private', False), 'private'),
            (lambda s: setattr(s, 'commit', 'b' * 40), 'commit'),
            (lambda s: s.release.update(prerelease=False), 'prerelease'),
            (lambda s: s.release['assets'][0].update(digest='wrong'), 'asset differs'),
            (lambda s: s.release['assets'].pop(), 'incomplete'),
        ]:
            with self.subTest(cause=cause):
                service = ReleaseAPI(self.directory, existing=True)
                change(service)
                with patch.object(publish_beta, 'api', service), self.assertRaisesRegex(ValueError, cause):
                    publish_beta.publish(TAG, COMMIT, self.directory)
                self.assertEqual(service.writes, [])

    def test_upload_bad_ack_keeps_draft(self):
        service = ReleaseAPI(self.directory)
        service.bad_ack = True
        with patch.object(publish_beta, 'api', service), self.assertRaisesRegex(ValueError, 'acknowledgement'):
            publish_beta.publish(TAG, COMMIT, self.directory)
        self.assertTrue(service.release['draft'])
        self.assertNotIn('PATCH', [method for method, _ in service.writes])


if __name__ == '__main__':
    unittest.main()
