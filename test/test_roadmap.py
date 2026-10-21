"""Prevent development work from being presented as a released feature."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('roadmap_renderer', ROOT / 'tools/render_roadmap.py')
roadmap = importlib.util.module_from_spec(spec)
spec.loader.exec_module(roadmap)


class ProgressTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / 'docs/roadmap.json').read_text(encoding='utf-8'))

    def test_each_feature_has_one_stable_identity_and_evidence_stage(self):
        counts = roadmap.validate_data(self.data)
        self.assertEqual(sum(counts.values()), len(self.data['features']))
        self.assertFalse(set(counts) - set(roadmap.STATES))

    def test_development_confirmation_does_not_increase_released_count(self):
        row = self.data['features'][0]
        row['status'] = 'implemented'
        counts = roadmap.coverage_counts(self.data)
        row['status'] = 'verified'
        after = roadmap.coverage_counts(self.data)
        self.assertEqual(after['released'], counts['released'])
        self.assertEqual(sum(after.values()), sum(counts.values()))
        self.assertEqual(after['verified'], counts['verified'] + 1)

    def test_promotion_without_matching_evidence_fails(self):
        row = self.data['features'][0]
        row['status'] = 'verified'
        with self.assertRaisesRegex(ValueError, 'differs from its evidence'):
            roadmap.validate_data(self.data)

    def test_duplicate_identity_or_name_fails_even_with_different_status(self):
        for key in ('id', 'name'):
            with self.subTest(key=key):
                data = copy.deepcopy(self.data)
                data['features'][1][key] = data['features'][0][key]
                with self.assertRaisesRegex(ValueError, 'Duplicate feature'):
                    roadmap.validate_data(data)

    def test_unknown_status_group_and_missing_evidence_fail(self):
        for key, value in (('status', 'partial'), ('group', 'unknown'), ('evidence', 'missing')):
            with self.subTest(key=key):
                data = copy.deepcopy(self.data)
                data['features'][0][key] = value
                with self.assertRaises(ValueError):
                    roadmap.validate_data(data)

    def test_empty_inventory_and_invalid_cells_fail(self):
        with self.assertRaises(ValueError):
            roadmap.coverage_counts({'features': []})
        for value in ('', 'one|two', 'one\ntwo', 'one\rtwo'):
            with self.subTest(value=value):
                data = copy.deepcopy(self.data)
                data['features'][0]['scope'] = value
                with self.assertRaises(ValueError):
                    roadmap.validate_data(data)

    def test_public_development_evidence_matches_the_inventory(self):
        evidence = json.loads((ROOT / 'docs/evidence/development-progress.json').read_text(encoding='utf-8'))
        self.assertFalse(evidence['development_features_are_released'])
        self.assertFalse(evidence['implemented']['in_game_success_claimed'])
        confirmed = {feature for milestone in evidence['confirmed']['milestones'] for feature in milestone['features']}
        implemented = {row['id'] for row in evidence['implemented']['features']}
        self.assertEqual(confirmed, {row['id'] for row in self.data['features'] if row['status'] == 'verified'})
        self.assertEqual(implemented, {row['id'] for row in self.data['features'] if row['status'] == 'implemented'})
        self.assertFalse(confirmed & implemented)

    def test_recent_progress_separates_download_and_development(self):
        row = self.data['features'][0]
        row['status'] = 'verified'
        recent = roadmap.recent_progress(self.data)
        self.assertIn('New in ' + self.data['version_label'], recent)
        self.assertIn('development only', recent)
        download, development = recent.split('### Confirmed since')
        for row in self.data['features']:
            if row['status'] == 'verified':
                self.assertIn(row['name'], development)
                self.assertNotIn(row['name'], download)

    def test_shipped_experiments_do_not_claim_confirmed_game_behavior(self):
        text = roadmap.status_key(self.data)
        self.assertIn('as experimental', text)
        self.assertIn('End-to-end game confirmation remains incomplete', text)
        recent = roadmap.recent_progress(self.data)
        self.assertIn('Experimental - included in this download', recent)
        self.assertTrue(any(row['status'] == 'experimental' for row in self.data['features']))

    def test_desktop_and_mobile_plot_every_feature_once_within_the_canvas(self):
        namespace = {'svg': 'http://www.w3.org/2000/svg'}
        expected = {row['id'] for row in self.data['features']}
        for mobile in (False, True):
            with self.subTest(mobile=mobile):
                root = ET.fromstring(roadmap.coverage_graphic(self.data, mobile=mobile))
                width, height = float(root.attrib['width']), float(root.attrib['height'])
                segments = root.findall('.//svg:rect[@data-feature]', namespace)
                self.assertEqual(len(segments), len(expected))
                self.assertEqual({item.attrib['data-feature'] for item in segments}, expected)
                for item in segments:
                    x, y = float(item.attrib['x']), float(item.attrib['y'])
                    self.assertGreaterEqual(x, 0)
                    self.assertGreaterEqual(y, 0)
                    self.assertLessEqual(x + float(item.attrib['width']), width)
                    self.assertLessEqual(y + float(item.attrib['height']), height)

    def test_every_feature_is_in_the_text_with_its_real_stage(self):
        text = roadmap.roadmap_text(self.data)
        for row in self.data['features']:
            self.assertIn('| ' + row['name'] + ' | ' + roadmap.STATES[row['status']], text)
        self.assertIn('not comparable', text)
        self.assertNotIn('Partial', text)


if __name__ == '__main__':
    unittest.main()
