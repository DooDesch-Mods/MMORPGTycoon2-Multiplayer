"""Reject unsafe release acknowledgements and writes of the beta publisher."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.github/scripts'))
import publish_beta

TAG = 'v0.1.0-beta.1'
COMMIT = 'a' * 40


def package(path):
    # The archive content is checked by test_framework_beta.py; here only the upload behavior matters.
    with zipfile.ZipFile(path, 'w') as zipped:
        zipped.writestr('package.json', json.dumps({'candidate': TAG + '@' + COMMIT}))
    path.with_suffix('.zip.sha256').write_text(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n')


class ReleaseAPI:
    def __init__(self, directory, existing=False):
        self.private = True
        self.repository = publish_beta.REPOSITORY
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
        if method == 'POST' and url.endswith('/releases/generate-notes'):
            return {'body': '**Full Changelog**: https://github.com/private/compare/previous...current'}
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
        return {'private': self.private, 'full_name': self.repository}


class BetaReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.archive = self.directory / ('MMORPGTycoon2-Multiplayer-' + TAG + '-windows-x64.zip')
        package(self.archive)
        validator = patch.object(publish_beta, 'validate')
        validator.start()
        self.addCleanup(validator.stop)

    def test_new_release_publishes_after_both_uploads(self):
        service = ReleaseAPI(self.directory)
        with patch.object(publish_beta, 'api', service):
            publish_beta.publish(TAG, COMMIT, self.directory)
        self.assertEqual([method for method, _ in service.writes], ['POST', 'POST', 'POST', 'POST', 'PATCH'])
        self.assertFalse(service.release['draft'])

    def test_identical_publish_does_not_write_again(self):
        service = ReleaseAPI(self.directory, existing=True)
        with patch.object(publish_beta, 'api', service):
            publish_beta.publish(TAG, COMMIT, self.directory)
        self.assertEqual(service.writes, [])

    def test_public_repository_can_publish_a_checked_prerelease(self):
        service = ReleaseAPI(self.directory)
        service.private = False
        with patch.object(publish_beta, 'api', service):
            publish_beta.publish(TAG, COMMIT, self.directory)
        self.assertFalse(service.release['draft'])
        self.assertTrue(service.release['prerelease'])

    def test_guard_failures_do_not_publish(self):
        for change, cause in [
            (lambda s: setattr(s, 'repository', 'other/repository'), 'repository'),
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
