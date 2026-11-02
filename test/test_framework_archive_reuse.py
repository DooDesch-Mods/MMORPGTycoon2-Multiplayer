import importlib.util
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

REPOSITORY = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPOSITORY / 'tools'))
spec = importlib.util.spec_from_file_location('framework_local_test', REPOSITORY / 'tools/framework_local_test.py')
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)


class ArchiveReuse(unittest.TestCase):
    def test_same_and_changed_archives_and_mutable_copies(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            base = Path(temp)
            root = base / '_MPTest' / 'lab'
            root.mkdir(parents=True)
            source = base / 'source.zip'
            mutable = base / 'settings.json'
            source.write_bytes(b'archive one')
            mutable.write_bytes(b'original settings')
            files = {'game/Data/MMORPG.zip': source, 'profile/settings.json': mutable}
            one, two, three = (root / name for name in ('one', 'two', 'three'))
            launcher.prepare_instance(one, root, files)
            launcher.prepare_instance(two, root, files)
            first = one / 'game/Data/MMORPG.zip'
            second = two / 'game/Data/MMORPG.zip'
            self.assertTrue(os.path.samefile(first, second))
            self.assertFalse(os.path.samefile(first, source))
            self.assertEqual(first.read_bytes(), b'archive one')
            self.assertFalse(os.path.samefile(one / 'profile/settings.json', two / 'profile/settings.json'))
            (one / 'profile/settings.json').write_bytes(b'changed')
            self.assertEqual((two / 'profile/settings.json').read_bytes(), b'original settings')
            source.write_bytes(b'archive two')
            launcher.prepare_instance(three, root, files)
            third = three / 'game/Data/MMORPG.zip'
            self.assertEqual(third.read_bytes(), b'archive two')
            self.assertEqual(first.read_bytes(), b'archive one')
            self.assertFalse(os.path.samefile(first, third))
            self.assertEqual(len(list((root.parent / 'immutable-archives').glob('*.zip'))), 2)

    def test_link_failure_falls_back_visibly(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parent) as temp:
            base = Path(temp)
            root = base / 'lab'
            root.mkdir()
            source = base / 'source.zip'
            source.write_bytes(b'archive')
            destination = base / 'out.zip'
            with patch.object(launcher.os, 'link', side_effect=OSError('unsupported link')), patch.object(launcher, 'log') as log:
                launcher.copy_immutable_archive(source, destination, root, launcher.sha256(source))
            self.assertEqual(destination.read_bytes(), b'archive')
            self.assertFalse(os.path.samefile(source, destination))
            log.assert_called_once()
            self.assertEqual(log.call_args.args[0], 'warn')
            seed = base / 'seed.zip'
            launcher.copy_immutable_archive(source, seed, root, launcher.sha256(source))
            other = base / 'other.zip'
            with patch.object(launcher.os, 'link', side_effect=OSError('cross-volume link')), patch.object(launcher, 'log') as log:
                launcher.copy_immutable_archive(source, other, root, launcher.sha256(source))
            self.assertEqual(other.read_bytes(), b'archive')
            self.assertFalse(os.path.samefile(seed, other))
            self.assertEqual(log.call_args.args[0], 'warn')


if __name__ == '__main__':
    unittest.main()
