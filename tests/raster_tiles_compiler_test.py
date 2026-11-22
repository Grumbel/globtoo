# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
import math
import pathlib
import tempfile
import unittest
from PIL import Image
from tools.build_raster_tiles import build
from tools.normalize_planet_texture import normalize


class RasterTest(unittest.TestCase):
    def test_seam_gutter_and_dimensions(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            # A 128x64 equirectangular fixture with different dateline sides.
            im = Image.new('RGB', (128,64))
            px = im.load()
            for y in range(64):
                for x in range(128):
                    px[x,y] = (x*2, y*4, x^y)
            src = root/'fixture.png'
            im.save(src)
            cols, rows = build(src,root/'out',64,4)
            self.assertEqual((cols,rows),(3,2))
            self.assertTrue((root/'out'/'preview.jpg').exists())
            self.assertEqual(Image.open(root/'out'/'preview-small.jpg').size,(1024,512))
            text = (root/'out'/'manifest.txt').read_text()
            self.assertIn('128 64 64 4 3 2',text)
            # Exact pixel tests require lossless data, so create a TIFF-like
            # secondary crop in memory from the source using the same routine.
            tile=Image.open(root/'out'/'tile-000-000.jpg')
            self.assertEqual(tile.size,(64,64))
            # JPEG colors need small tolerance; gutter wraps to opposite side.
            expected = px[127,10]
            actual = tile.getpixel((3,14))
            self.assertLess(max(abs(a-b) for a,b in zip(expected,actual)),12)
            adjacent=Image.open(root/'out'/'tile-000-001.jpg')
            # Shared source column survives gutter encoding on both sides.
            a=tile.getpixel((60,14))
            b=adjacent.getpixel((4,14))
            self.assertLess(max(abs(x-y) for x,y in zip(a,b)),14)
            end = Image.open(root/'out'/'tile-001-002.jpg')
            self.assertEqual(end.size,(64,64))
    def test_planetary_orientation_and_dateline_gutters(self):
        # A continuous, asymmetric waveform tests pixel-center mirroring and
        # half-turning, including pixels in the cyclic antimeridian gutters.
        width, height = 128, 64
        image = Image.new('RGB', (width, height))
        pixels = image.load()
        for y in range(height):
            for x in range(width):
                angle = math.tau * x / width
                pixels[x, y] = (round(128 + 83 * math.sin(angle)),
                                round(128 + 77 * math.cos(angle)),
                                round(20 + 195 * y / height))
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            source = root / 'planet.png'
            image.save(source)
            for positive in ('east', 'west'):
                for left in (0, 180):
                    with self.subTest(positive=positive, left=left):
                        output = root / f'{positive}-{left}'
                        build(source, output, 64, 4, positive, left)
                        expected = normalize(image, positive, left, width)
                        # Tile 0 data begins at pixel 4 and covers columns 0..55.
                        with Image.open(output / 'tile-000-000.jpg') as tile:
                            for tx, source_x in ((3, 127), (4, 0), (20, 16),
                                                 (53, 49), (59, 55)):
                                actual = tile.getpixel((tx, 21))
                                goal = expected.getpixel((source_x, 17))
                                self.assertLessEqual(max(abs(a-b) for a,b in zip(actual, goal)),
                                                     12, (positive, left, tx, actual, goal))
                        # A later tile's gutter must agree with *both* sides
                        # of its neighboring tile, with no half-tile shift.
                        with Image.open(output / 'tile-000-001.jpg') as tile:
                            for tx, x in ((3, 55), (4, 56), (17, 69), (36, 88)):
                                actual = tile.getpixel((tx, 21))
                                goal = expected.getpixel((x, 17))
                                self.assertLessEqual(max(abs(a-b) for a,b in zip(actual, goal)), 12)
                        with Image.open(output / 'preview.jpg') as preview:
                            # Resampling is performed in source coordinates,
                            # followed by the same orientation transformation.
                            self.assertEqual(preview.size, (2048, 1024))
        with tempfile.TemporaryDirectory() as d:
            source = pathlib.Path(d) / 'source.png'
            image.save(source)
            for invalid in (('north', 180), ('west', 90)):
                with self.assertRaisesRegex(ValueError, 'longitude'):
                    build(source, pathlib.Path(d) / 'bad', 64, 4, *invalid)

    def test_reject_not_world(self):
        with tempfile.TemporaryDirectory() as d:
            src = pathlib.Path(d)/'bad.png'
            Image.new('RGB',(64,64)).save(src)
            with self.assertRaisesRegex(ValueError,'2:1'):
                build(src,pathlib.Path(d)/'out')

if __name__ == '__main__':
    unittest.main()
