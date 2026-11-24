#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Hermetic end-to-end tests for operator-supplied planetary map imports."""

import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

from tools.prepare_planetary_map import prepare, hash_file
from tools.planetary_alignment import pixel_center


class PlanetaryMapPreparationTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'io-source.png'
        self.destination = self.root / 'io-tiles'
        # Non-symmetric synthetic image: west-positive map with longitude
        # 0 at left.  Check QA centers against normalized viewer placement.
        image = Image.new('RGB', (128, 64))
        pixels = image.load()
        for y in range(64):
            for x in range(128):
                pixels[x, y] = ((x * 3 + y) % 256, (x + y * 2) % 256, (y * 4) % 256)
        image.save(self.source)
        self.settings = dict(body='io', source_url='https://example.invalid/io/source',
                             license_text='Test fixture (not externally verified)',
                             projection='plate-carree', positive='west', left=0,
                             latitude='planetographic', tile_size=64, gutter=4,
                             qa_width=256)

    def test_end_to_end_manifest_hashes_and_feature_alignment(self):
        original_hash = hash_file(self.source)[0]
        record = prepare(self.source, self.destination,
                         expected_sha256=original_hash.upper(), **self.settings)
        self.assertTrue(record['source_sha256_verified_against_supplied_pin'])
        self.assertEqual(record['source_sha256'], original_hash)
        self.assertEqual(record['source_width'], 128)
        self.assertEqual(record['source_longitude_positive_as_declared'], 'west')
        self.assertEqual(record['source_latitude_type_as_declared'], 'planetographic')
        self.assertEqual(record['body'], 'io')
        self.assertEqual(record['schema'], 'globtoo-planetary-source-v1')
        self.assertTrue(set(record['files_sha256']).issuperset({
            'manifest.txt', 'preview.jpg', 'preview-small.jpg',
            'tile-000-000.jpg', 'alignment.png', 'alignment.json'}))
        self.assertTrue((self.destination / 'source.txt').exists())
        self.assertEqual(json.loads((self.destination / 'source.json').read_text()), record)
        self.assertTrue((self.destination / 'manifest.txt').read_text().startswith('GLOBTOO_RASTER_TILES 1'))
        for name, digest in record['files_sha256'].items():
            self.assertEqual(hash_file(self.destination / name)[0], digest)
        qa = json.loads((self.destination / 'alignment.json').read_text())
        self.assertEqual(qa['body'], 'io')
        self.assertEqual(qa['input'], str(self.destination))
        self.assertEqual(qa['image_width'], 256)
        for feature in qa['features']:
            self.assertEqual((feature['x'], feature['y']),
                             pixel_center(feature['longitude_east'], feature['latitude'], 256, 128))
        self.assertEqual(hash_file(self.source)[0], original_hash)  # input unchanged
        # A map whose payload is only hashed locally must not claim verification.
        self.assertFalse(prepare(self.source, self.destination, replace=True,
                                 **self.settings)['source_sha256_verified_against_supplied_pin'])

    def test_hash_mismatch_and_bad_metadata_leave_previous_tree_untouched(self):
        self.destination.mkdir()
        (self.destination / 'old.txt').write_text('keep this one')
        with self.assertRaisesRegex(ValueError, 'SHA-256 mismatch'):
            prepare(self.source, self.destination, replace=True,
                    expected_sha256='0' * 64, **self.settings)
        self.assertEqual((self.destination / 'old.txt').read_text(), 'keep this one')
        with self.assertRaisesRegex(ValueError, 'global plate-carree'):
            prepare(self.source, self.destination, replace=True,
                    **{**self.settings, 'projection': 'mercator'})
        with self.assertRaisesRegex(ValueError, 'longitude'):
            prepare(self.source, self.destination, replace=True,
                    **{**self.settings, 'positive': 'unknown'})
        with self.assertRaisesRegex(ValueError, 'license'):
            prepare(self.source, self.destination, replace=True,
                    **{**self.settings, 'license_text': ''})
        self.assertEqual(sorted(p.name for p in self.destination.iterdir()), ['old.txt'])
        self.assertFalse(any(self.root.glob('.io-tiles.staging-*')))

    def test_partial_conversion_does_not_publish_or_destroy_prior_map(self):
        self.destination.mkdir()
        (self.destination / 'old.txt').write_text('still here')
        def failed_build(source, destination, *args):
            (destination / 'partial.jpg').write_bytes(b'incomplete')
            raise OSError('simulated disk failure')
        with patch('tools.prepare_planetary_map.build', side_effect=failed_build):
            with self.assertRaisesRegex(OSError, 'disk failure'):
                prepare(self.source, self.destination, replace=True, **self.settings)
        self.assertEqual((self.destination / 'old.txt').read_text(), 'still here')
        self.assertFalse(any(self.root.glob('.io-tiles.staging-*')))
        self.assertFalse(any(self.root.glob('.io-tiles.backup-*')))

    def test_rename_failure_restores_prior_directory(self):
        self.destination.mkdir()
        (self.destination / 'old.txt').write_text('previous generation')
        original_replace = os.replace
        def fail_publish(source, target):
            if '.staging-' in str(source):
                raise OSError('simulated rename failure')
            return original_replace(source, target)
        with patch('tools.prepare_planetary_map.os.replace', side_effect=fail_publish):
            with self.assertRaisesRegex(OSError, 'rename failure'):
                prepare(self.source, self.destination, replace=True, **self.settings)
        self.assertEqual((self.destination / 'old.txt').read_text(), 'previous generation')
        self.assertFalse(any(self.root.glob('.io-tiles.staging-*')))
        self.assertFalse(any(self.root.glob('.io-tiles.backup-*')))

    def test_reject_existing_output_without_opt_in_and_bad_image(self):
        self.destination.mkdir()
        with self.assertRaisesRegex(ValueError, 'already exists'):
            prepare(self.source, self.destination, **self.settings)
        self.source.unlink()
        Image.new('RGB', (128, 128)).save(self.source)
        with self.assertRaisesRegex(ValueError, '2:1'):
            prepare(self.source, self.destination, replace=True, **self.settings)
        self.assertEqual(list(self.destination.iterdir()), [])

    def test_reject_empty_or_malformed_hash_and_source_inside_output(self):
        with self.assertRaisesRegex(ValueError, 'exactly 64'):
            prepare(self.source, self.destination, expected_sha256='bad', **self.settings)
        self.destination.mkdir()
        nested = self.destination / 'nested.png'
        nested.write_bytes(self.source.read_bytes())
        with self.assertRaisesRegex(ValueError, 'inside output'):
            prepare(nested, self.destination, replace=True, **self.settings)
        self.assertEqual(nested.read_bytes(), self.source.read_bytes())


if __name__ == '__main__':
    unittest.main()
