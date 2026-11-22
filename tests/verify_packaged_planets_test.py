#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the package audit with real tile-compiler output and bad installs."""
from pathlib import Path
import shutil
import tempfile
import unittest

from PIL import Image

from tools.build_raster_tiles import build
from tools.verify_packaged_planets import verify_body
from tools.prepare_planetary_map import prepare


class PackagedPlanetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.prefix = Path(self.tmp.name) / 'prefix'
        self.data = self.prefix / 'share' / 'globtoo'
        self.imagery = self.data / 'imagery' / 'moon'
        self.imagery.mkdir(parents=True)
        source = Path(self.tmp.name) / 'source.jpg'
        Image.new('RGB', (128, 64), (70, 35, 120)).save(source)
        build(source, self.imagery, tile_size=64)
        (self.imagery / 'source.txt').write_text(
            'Source: https://example.invalid/test\nLicense: synthetic fixture\n'
            'Original longitude direction: east\nSource latitude type: planetocentric\n'
            'Output: north-up planetocentric\n', encoding='utf-8')
        assets = self.data / 'assets'
        assets.mkdir()
        self.base = assets / 'moon.jpg'
        shutil.copyfile(self.imagery / 'preview-small.jpg', self.base)

    def test_generated_map_passes(self):
        self.assertEqual(verify_body(self.prefix, 'moon'), 6)

    def test_missing_and_extra_tiles_fail(self):
        tile = next(self.imagery.glob('tile-*.jpg'))
        tile.unlink()
        with self.assertRaisesRegex(ValueError, 'tile grid'):
            verify_body(self.prefix, 'moon')
        Image.new('RGB', (64, 64)).save(tile)
        Image.new('RGB', (64, 64)).save(self.imagery / 'tile-002-020.jpg')
        with self.assertRaisesRegex(ValueError, 'tile grid'):
            verify_body(self.prefix, 'moon')

    def test_corrupted_tile_fails(self):
        next(self.imagery.glob('tile-*.jpg')).write_bytes(b'not an image')
        with self.assertRaises(OSError):
            verify_body(self.prefix, 'moon')

    def test_inconsistent_base_fails(self):
        Image.new('RGB', (1024, 512), 'green').save(self.base)
        with self.assertRaisesRegex(ValueError, 'base texture does not match'):
            verify_body(self.prefix, 'moon')

    def test_bad_manifest_fails(self):
        (self.imagery / 'manifest.txt').write_text('GLOBTOO_RASTER_TILES 1\n128 64 64 4 1 1\n')
        with self.assertRaisesRegex(ValueError, 'impossible tile layout'):
            verify_body(self.prefix, 'moon')

    def test_missing_attribution_fails(self):
        (self.imagery / 'source.txt').write_text('Source: example\n')
        with self.assertRaisesRegex(ValueError, 'attribution'):
            verify_body(self.prefix, 'moon')

    def test_prepared_map_source_json_hashes(self):
        source = Path(self.tmp.name) / 'prepared-source.png'
        Image.new('RGB', (128, 64), 'gold').save(source)
        prepared = Path(self.tmp.name) / 'prepared'
        prepare(source, prepared, body='mars',
                source_url='https://example.invalid/mars', license_text='synthetic fixture',
                positive='east', left=180, latitude='planetocentric',
                projection='plate-carree', tile_size=64, qa_width=256)
        target = self.data / 'imagery' / 'mars'
        shutil.copytree(prepared, target)
        shutil.copyfile(target / 'preview-small.jpg', self.data / 'assets' / 'mars.jpg')
        self.assertEqual(verify_body(self.prefix, 'mars'), 6)
        (target / 'tile-000-000.jpg').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
            verify_body(self.prefix, 'mars')


if __name__ == '__main__':
    unittest.main()
