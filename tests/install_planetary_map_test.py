#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Offline planetary package integrity, XDG install and replacement tests."""

import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

from tools.install_planetary_map import data_root, verify_prepared_map, install_map
from tools.prepare_planetary_map import prepare


class PlanetaryMapInstallationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data_home = self.root / 'xdg'
        image = self.root / 'moon-source.png'
        Image.new('RGB', (128, 64), (102, 88, 57)).save(image)
        self.prepared = self.root / 'prepared'
        prepare(image, self.prepared, body='moon',
                source_url='https://example.invalid/moon',
                license_text='synthetic test fixture',
                positive='east', left=180, latitude='planetocentric',
                projection='plate-carree', tile_size=64, qa_width=256)
        self.installed = self.data_home / 'globtoo' / 'imagery' / 'moon'

    def test_verify_and_install_preserves_manifest_hashes(self):
        record = verify_prepared_map(self.prepared, 'moon')
        self.assertIn('source.txt', record['files_sha256'])
        self.assertEqual(install_map(self.prepared, data_home=self.data_home), self.installed)
        self.assertEqual(verify_prepared_map(self.installed, 'moon'), record)
        self.assertFalse((self.data_home / 'globtoo' / 'imagery' / 'mars').exists())
        self.assertFalse(any(self.installed.parent.glob('.moon.*')))
        with self.assertRaisesRegex(ValueError, 'already installed'):
            install_map(self.prepared, data_home=self.data_home)
        self.assertEqual(install_map(self.prepared, data_home=self.data_home,
                                     replace=True), self.installed)

    def test_reject_corruption_symlink_and_untracked_files_without_install(self):
        tile = next(self.prepared.glob('tile-*.jpg'))
        pristine = tile.read_bytes()
        tile.write_bytes(b'corrupted')
        with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
            install_map(self.prepared, data_home=self.data_home)
        self.assertFalse(self.installed.exists())
        tile.write_bytes(pristine)
        (self.prepared / 'surprise.txt').write_text('not audited')
        with self.assertRaisesRegex(ValueError, 'unexpected or missing'):
            verify_prepared_map(self.prepared)
        (self.prepared / 'surprise.txt').unlink()
        tile.unlink()
        tile.symlink_to(self.root / 'moon-source.png')
        with self.assertRaisesRegex(ValueError, 'non-regular'):
            verify_prepared_map(self.prepared)

    def test_body_and_manifest_consistency(self):
        with self.assertRaisesRegex(ValueError, 'body mismatch'):
            verify_prepared_map(self.prepared, 'mars')
        record_file = self.prepared / 'source.json'
        record = json.loads(record_file.read_text())
        record['output_coordinates'] = 'unknown'
        record_file.write_text(json.dumps(record))
        with self.assertRaisesRegex(ValueError, 'coordinate convention'):
            verify_prepared_map(self.prepared)

    def test_failed_copy_and_publication_preserve_existing_install(self):
        self.installed.parent.mkdir(parents=True)
        self.installed.mkdir()
        (self.installed / 'old.txt').write_text('original')
        with patch('tools.install_planetary_map.shutil.copy2', side_effect=OSError('disk full')):
            with self.assertRaisesRegex(OSError, 'disk full'):
                install_map(self.prepared, data_home=self.data_home, replace=True)
        self.assertEqual((self.installed / 'old.txt').read_text(), 'original')
        original_replace = os.replace
        def publish_fails(src, dst):
            if '.staging-' in str(src):
                raise OSError('rename failed')
            return original_replace(src, dst)
        with patch('tools.install_planetary_map.os.replace', side_effect=publish_fails):
            with self.assertRaisesRegex(OSError, 'rename failed'):
                install_map(self.prepared, data_home=self.data_home, replace=True)
        self.assertEqual((self.installed / 'old.txt').read_text(), 'original')
        self.assertFalse(any(self.installed.parent.glob('.moon.staging-*')))
        self.assertFalse(any(self.installed.parent.glob('.moon.backup-*')))

    def test_corrupted_existing_install_does_not_block_explicit_replacement(self):
        self.installed.mkdir(parents=True)
        (self.installed / 'bad.txt').write_text('old')
        install_map(self.prepared, data_home=self.data_home, replace=True)
        self.assertEqual(verify_prepared_map(self.installed)['body'], 'moon')
        self.assertFalse((self.installed / 'bad.txt').exists())

    def test_xdg_root_rules(self):
        with patch.dict(os.environ, {'XDG_DATA_HOME': str(self.data_home)}):
            self.assertEqual(data_root(), self.data_home / 'globtoo')
        with patch.dict(os.environ, {'XDG_DATA_HOME': 'relative',
                                    'HOME': str(self.root)}, clear=True):
            self.assertEqual(data_root(), self.root / '.local/share/globtoo')
        with self.assertRaisesRegex(ValueError, 'absolute'):
            data_root(Path('relative'))
        with patch('tools.install_planetary_map.data_root', return_value=self.prepared):
            with self.assertRaisesRegex(ValueError, 'overlap'):
                install_map(self.prepared, data_home=self.data_home)



if __name__ == '__main__':
    unittest.main()
