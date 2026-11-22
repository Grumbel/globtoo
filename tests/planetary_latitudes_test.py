# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Mars geodetic/centric conversion and tile/single-image parity checks."""
import math
from pathlib import Path
import tempfile
import unittest

from PIL import Image

from tools.planetary_latitudes import (MARS_EQUATORIAL_KM, MARS_POLAR_KM,
                                       source_row)
from tools.normalize_planet_texture import normalize
from tools.build_raster_tiles import build


class PlanetaryLatitudesTest(unittest.TestCase):
    def test_mars_biaxial_formula_and_poles(self):
        h = 1800
        for latitude_c in (0.0, 20.0, 45.0, 70.0, -20.0, -45.0, -70.0):
            # y is allowed fractional here to independently test coordinates
            y = (90.0 - latitude_c) * h / 180.0 - 0.5
            actual = source_row(y, h, 'mars', 'planetographic')
            latitude_g = 90.0 - (actual + 0.5) * 180.0 / h
            expected = math.degrees(math.atan2(
                (MARS_EQUATORIAL_KM / MARS_POLAR_KM)**2 *
                math.sin(math.radians(latitude_c)),
                math.cos(math.radians(latitude_c))))
            self.assertAlmostEqual(latitude_g, expected, places=10)
            if latitude_c > 0:
                self.assertGreater(latitude_g, latitude_c)
            if latitude_c < 0:
                self.assertLess(latitude_g, latitude_c)
        # At 45 degrees Mars's planetographic latitude differs by ~0.339deg.
        y45 = (90.0 - 45.0) * h / 180.0 - 0.5
        self.assertAlmostEqual(
            90.0 - (source_row(y45, h, 'mars', 'planetographic') + 0.5) * 180.0 / h,
            45.3385, delta=0.002)
        self.assertEqual(source_row(-200, h, 'mars', 'planetographic'), 0)
        self.assertEqual(source_row(h+200, h, 'mars', 'planetographic'), h-1)
        # USGS/IAU Gazetteer uses a spherical reference for the Moon and Io.
        for body in ('moon', 'io'):
            for y in (0, 311, h-1):
                self.assertEqual(source_row(y, h, body, 'planetographic'), y)
        self.assertEqual(source_row(311, h, 'mars', 'planetocentric'), 311)

    def test_mars_raster_and_tiled_output_agree(self):
        w, h = 720, 360
        image = Image.new('RGB', (w, h))
        pix = image.load()
        for y in range(h):
            for x in range(w):
                wave = math.tau * x / w
                pix[x, y] = (round(120 + 75*math.sin(wave)),
                             round(120 + 75*math.cos(wave)),
                             round(15 + 220 * y / (h - 1)))
        with tempfile.TemporaryDirectory(prefix='globtoo-mars-lat-') as d:
            root = Path(d)
            source = root / 'mars.png'
            image.save(source)
            for longitude_positive, left in (('west', 0), ('east', 180)):
                with self.subTest(longitude_positive=longitude_positive, left=left):
                    output = root / f'{longitude_positive}-{left}'
                    build(source, output, 128, 4, longitude_positive, left,
                          'planetographic', 'mars')
                    expected = normalize(image, longitude_positive, left, w,
                                         'planetographic', 'mars')
                    # Check both ordinary tile interiors and the dateline gutter.
                    content = 120
                    for x, y in ((0, 90), (119, 130), (120, 131),
                                 (w-1, h-1), (365, 180), (2, 100)):
                        column, row = x // content, y // content
                        with Image.open(output / f'tile-{row:03d}-{column:03d}.jpg') as tile:
                            actual = tile.getpixel((x % content + 4,
                                                    y % content + 4))
                        wanted = expected.getpixel((x, y))
                        self.assertLessEqual(max(abs(a-b) for a,b in zip(actual,wanted)), 12)
                    self.assertIn('720 360 128 4',
                                  (output / 'manifest.txt').read_text())
        # A Mars graphic->centric shift is measurable away from the equator.
        self.assertNotAlmostEqual(source_row(90, h, 'mars', 'planetographic'), 90)

    def test_reject_ambiguous_source_type(self):
        with self.assertRaisesRegex(ValueError, 'needs --body'):
            normalize(Image.new('RGB', (8, 4)), 'east', 180, 8,
                      'planetographic')
        with self.assertRaisesRegex(ValueError, 'needs --body'):
            build('/nonexistent', '/tmp/out', latitude='planetographic')
        for body in ('moon', 'io'):
            im = Image.new('RGB', (8,4), (10,20,30))
            self.assertEqual(normalize(im, 'east', 180, 8,
                                       'planetographic', body).tobytes(), im.tobytes())


if __name__ == '__main__':
    unittest.main()
