"""Check explicit milestone admission and exact source tag binding without writes."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SOURCE = next(path for path in [ROOT, *ROOT.parents] if (path / '.github/scripts/build_beta.py').is_file())
sys.path.insert(0, str(SOURCE / '.github/scripts'))
sys.path.insert(0, str(ROOT / '.github/scripts'))
import beta_milestone
import publish_beta

COMMIT = 'a' * 40
TAG = 'v0.1.0-beta.3'


class TagAPI:
    def __init__(self, existing=None):
        self.private = True
        self.existing = existing
        self.remote_commit = COMMIT
        self.bad_ack = False
        self.timeout = False
        self.calls = []

    def __call__(self, method, url, payload=None, missing=False):
        self.calls.append((method, url, payload))
        if '/git/ref/tags/' in url:
            return copy.deepcopy(self.existing)
        if '/git/commits/' in url or '/commits/' in url:
            return {'sha': self.remote_commit}
        if method == 'POST':
            if self.timeout:
                raise TimeoutError('original tag request timed out')
            self.existing = {'ref': 'refs/tags/' + TAG, 'object': {'type': 'commit', 'sha': COMMIT}}
            if self.bad_ack:
                self.existing['object']['sha'] = 'b' * 40
            return copy.deepcopy(self.existing)
        return {'private': self.private}

    def writes(self):
        return [call for call in self.calls if call[0] != 'GET']


class MilestoneTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.record = self.root / beta_milestone.RECORD
        self.record.parent.mkdir()
        self.data = {'schema': 1, 'version': TAG[1:], 'summary': 'Accepted test fixture',
                     'native_evidence': 'retained/unit-fixture-proof.json'}
        self.save()

    def save(self, data=None):
        self.record.write_text(json.dumps(self.data if data is None else data), encoding='utf8')

    def test_main_uses_explicit_version(self):
        self.assertEqual(beta_milestone.resolve(self.root, 'refs/heads/main'), (TAG, True))

    def test_manual_tag_needs_no_record(self):
        self.record.unlink()
        self.assertEqual(beta_milestone.resolve(self.root, 'refs/tags/v0.1.0-beta.2'), ('v0.1.0-beta.2', False))

    def test_main_without_acceptance_record_fails(self):
        self.record.unlink()
        with self.assertRaises(FileNotFoundError):
            beta_milestone.resolve(self.root, 'refs/heads/main')

    def test_other_branch_or_nonbeta_ref_rejected(self):
        for ref in ['refs/heads/feature', 'refs/tags/v0.1.0', 'refs/tags/v0.1.0-beta.0', 'refs/tags/v0.1.0-beta.3\nsecond=value']:
            with self.subTest(ref=ref), self.assertRaises(ValueError):
                beta_milestone.resolve(self.root, ref)

    def test_missing_unknown_and_wrong_typed_fields_rejected(self):
        for field in self.data:
            bad = dict(self.data)
            del bad[field]
            self.save(bad)
            with self.subTest(missing=field), self.assertRaises(ValueError):
                beta_milestone.read_record(self.record)
        for field, value in [('schema', True), ('schema', 2), ('version', 'auto'), ('summary', ''),
                             ('native_evidence', ''), ('native_evidence', []), ('summary', 'one\ntwo'), ('extra', 'unexpected')]:
            bad = dict(self.data, **{field: value})
            self.save(bad)
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                beta_milestone.read_record(self.record)

    def test_duplicate_json_and_oversized_record_rejected(self):
        self.record.write_text('{"schema":1,"schema":1}')
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            beta_milestone.read_record(self.record)
        self.record.write_text(' ' * 8193)
        with self.assertRaisesRegex(ValueError, '8192'):
            beta_milestone.read_record(self.record)

    def test_new_tag_uses_exact_built_commit_once(self):
        api = TagAPI()
        beta_milestone.ensure_tag(TAG, COMMIT, api)
        self.assertEqual(len(api.writes()), 1)
        self.assertEqual(api.writes()[0][2], {'ref': 'refs/tags/' + TAG, 'sha': COMMIT})
        self.assertEqual(api.calls[-1][1].split('/')[-2:], ['commits', TAG])

    def test_existing_same_commit_tag_is_read_only(self):
        for kind in ['commit', 'tag']:
            with self.subTest(kind=kind):
                api = TagAPI({'ref': 'refs/tags/' + TAG, 'object': {'type': kind, 'sha': COMMIT if kind == 'commit' else 'd' * 40}})
                beta_milestone.ensure_tag(TAG, COMMIT, api)
                self.assertEqual(api.writes(), [])

    def test_existing_beta2_tag_is_never_moved(self):
        api = TagAPI({'ref': 'refs/tags/v0.1.0-beta.2', 'object': {'type': 'commit', 'sha': 'b' * 40}})
        api.remote_commit = 'b' * 40
        with self.assertRaisesRegex(ValueError, 'preserved without overwrite'):
            beta_milestone.ensure_tag('v0.1.0-beta.2', COMMIT, api)
        self.assertEqual(api.writes(), [])

    def test_public_repo_or_unknown_source_cannot_create_tag(self):
        for field, value in [('private', False), ('remote_commit', 'b' * 40)]:
            api = TagAPI()
            setattr(api, field, value)
            with self.subTest(field=field), self.assertRaises(ValueError):
                beta_milestone.ensure_tag(TAG, COMMIT, api)
            self.assertEqual(api.writes(), [])

    def test_bad_creation_ack_stops_after_one_mutation(self):
        api = TagAPI()
        api.bad_ack = True
        with self.assertRaisesRegex(ValueError, 'acknowledgement differs'):
            beta_milestone.ensure_tag(TAG, COMMIT, api)
        self.assertEqual(len(api.writes()), 1)

    def test_unknown_tag_timeout_keeps_original_cause_without_retry(self):
        api = TagAPI()
        api.timeout = True
        with self.assertRaisesRegex(TimeoutError, 'original tag request timed out'):
            beta_milestone.ensure_tag(TAG, COMMIT, api)
        self.assertEqual(len(api.writes()), 1)

    def test_milestone_notes_and_source_reach_existing_publisher(self):
        spec = importlib.util.spec_from_file_location('release_fixtures', SOURCE / 'test/test_beta_release.py')
        fixtures = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(fixtures)
        self.data['version'] = fixtures.TAG[1:]
        self.save()
        assets = self.root / 'assets'
        assets.mkdir()
        archive = assets / ('MMORPGTycoon2-Multiplayer-' + fixtures.TAG + '-windows-x64.zip')
        fixtures.package(archive)
        api = fixtures.ReleaseAPI(assets)
        posts = []
        def capture(method, url, payload=None, *args, **kwargs):
            if method == 'POST' and url.endswith('/releases'):
                posts.append(payload)
            return api(method, url, payload, *args, **kwargs)
        with patch.object(publish_beta, 'api', capture), patch.object(publish_beta, '__file__', str(SOURCE / '.github/scripts/publish_beta.py')):
            publish_beta.publish(fixtures.TAG, fixtures.COMMIT, assets, self.record)
        self.assertIn(self.data['summary'], posts[0]['body'])
        self.assertIn(self.data['native_evidence'], posts[0]['body'])
        self.assertIn(fixtures.COMMIT, posts[0]['body'])
        self.assertFalse(api.release['draft'])
        self.assertTrue(api.release['prerelease'])

    def test_wrong_milestone_version_cannot_call_release_api(self):
        with patch.object(publish_beta, 'api') as api, self.assertRaisesRegex(ValueError, 'milestone version differs'):
            publish_beta.publish('v0.1.0-beta.2', COMMIT, self.root, self.record)
        api.assert_not_called()


if __name__ == '__main__':
    unittest.main()
