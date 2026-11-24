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

    def test_south_up_source_matches_rotated_north_up(self):
        # A Venus-style mosaic rotated by 180 degrees is south-up AND west-
        # positive; declaring both must reproduce the north-up east tiles.
        width, height = 128, 64
        image = Image.new('RGB', (width, height))
        pixels = image.load()
        for y in range(height):
            for x in range(width):
                pixels[x, y] = (round(255 * x / width), round(255 * y / height), 90)
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            image.save(root / 'north.png')
            image.rotate(180).save(root / 'south.png')
            build(root / 'north.png', root / 'n', 64, 4, 'east', 180)
            build(root / 'south.png', root / 's', 64, 4, 'west', 180, top_edge='south')
            for name in ('tile-000-000.jpg', 'tile-001-002.jpg', 'preview-small.jpg'):
                with Image.open(root / 'n' / name) as a, Image.open(root / 's' / name) as b:
                    for point in ((5, 5), (30, 40), (60, 10), (12, 60)):
                        pa, pb = a.getpixel(point), b.getpixel(point)
                        self.assertLessEqual(max(abs(u-v) for u, v in zip(pa, pb)), 12,
                                             (name, point, pa, pb))
            north = normalize(image, 'east', 180, width)
            south = normalize(image.rotate(180), 'west', 180, width, top='south')
            self.assertEqual(list(north.getdata()), list(south.getdata()))
            with self.assertRaisesRegex(ValueError, 'north or south'):
                build(root / 'north.png', root / 'bad', 64, 4, top_edge='east')

    def test_reject_not_world(self):
        with tempfile.TemporaryDirectory() as d:
            src = pathlib.Path(d)/'bad.png'
            Image.new('RGB',(64,64)).save(src)
            with self.assertRaisesRegex(ValueError,'2:1'):
                build(src,pathlib.Path(d)/'out')

    def test_pyramid_levels_and_manifest(self):
        # Sources wider than the 2048-pixel preview get coarser levels until
        # a level would no longer be sharper than that preview.
        from tools.raster_manifest import parse_manifest, pyramid_levels, tile_name
        self.assertEqual(pyramid_levels(2048, 1024), 1)
        self.assertEqual(pyramid_levels(8192, 4096), 2)
        self.assertEqual(pyramid_levels(21600, 10800), 4)
        width, height = 4352, 2176
        image = Image.new('RGB', (width, height))
        # West half red, east half blue: survives resampling at every level.
        image.paste((220, 30, 30), (0, 0, width // 2, height))
        image.paste((30, 30, 220), (width // 2, 0, width, height))
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            source = root / 'wide.png'
            image.save(source)
            build(source, root / 'out', 256, 4, positive='west', left=0)
            layout = parse_manifest((root / 'out' / 'manifest.txt').read_text())
            self.assertEqual((layout.levels, layout.columns, layout.rows), (2, 18, 9))
            self.assertEqual(layout.level_size(1), (2176, 1088))
            self.assertEqual(layout.level_grid(1), (9, 5))
            names = {p.name for p in (root / 'out').glob('tile-*.jpg')}
            self.assertEqual(names, layout.expected_tiles())
            self.assertIn(tile_name(1, 4, 8), names)
            # west-positive / 0-left source: its first half (red) covers
            # 0..180 W, which is the canonical *left* half after normalizing.
            first = Image.open(root / 'out' / tile_name(1, 2, 0)).getpixel((100, 100))
            last = Image.open(root / 'out' / tile_name(1, 2, 8)).getpixel((20, 100))
            self.assertGreater(first[0], 150)
            self.assertGreater(last[2], 150)

if __name__ == '__main__':
    unittest.main()
