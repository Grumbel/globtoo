# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
import math
import pathlib
import tempfile
import unittest
from PIL import Image
from tools.build_raster_tiles import build

try:
    import pyvips  # noqa: F401  optional large-raster backend
except ImportError:
    pyvips = None
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

    def test_brighten_ocean_keeps_land_and_ice(self):
        from tools.build_raster_tiles import OCEAN_DEEP, brighten_ocean
        samples = {
            'deep': (2, 5, 20), 'open': (7, 19, 47), 'shelf': (28, 70, 131),
            'forest': (40, 60, 25), 'desert': (200, 160, 110),
            'ice': (225, 235, 245), 'shadow': (40, 42, 46),
        }
        image = Image.new('RGB', (len(samples), 1))
        for x, color in enumerate(samples.values()):
            image.putpixel((x, 0), color)
        out = dict(zip(samples, brighten_ocean(image).getdata()))
        self.assertEqual(out['deep'], OCEAN_DEEP)
        self.assertLess(out['deep'][2], out['open'][2])
        self.assertLess(out['open'][2], out['shelf'][2])
        for name in ('forest', 'desert', 'ice', 'shadow'):
            self.assertEqual(out[name], samples[name], name)

    def test_recolor_and_base_from_same_source(self):
        from tools.build_raster_tiles import brighten_ocean
        from tools.raster_manifest import tile_name
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            source = root / 'dark.png'
            Image.new('RGB', (1024, 512), (7, 19, 47)).save(source)
            build(source, root / 'out', 256, 4, recolor=brighten_ocean,
                  base_width=512)
            base = Image.open(root / 'out' / 'base.jpg')
            self.assertEqual(base.size, (512, 256))
            for image in (base, Image.open(root / 'out' / 'preview.jpg'),
                          Image.open(root / 'out' / tile_name(0, 1, 1))):
                self.assertGreater(image.convert('RGB').getpixel((10, 10))[2], 100)
            with self.assertRaisesRegex(ValueError, 'even'):
                build(source, root / 'odd', 256, 4, base_width=511)

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

    @unittest.skipIf(pyvips is None, 'pyvips not installed')
    def test_vips_backend_and_mosaic_match_pillow(self):
        from PIL import ImageChops
        # Wide enough for a second pyramid level (above the 2048 preview).
        width, height = 4352, 2176
        gradient = Image.linear_gradient('L')
        im = Image.merge('RGB', (gradient.rotate(90).resize((width, height)),
                                 gradient.resize((width, height)),
                                 Image.effect_noise((width, height), 64)))
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            whole = root/'whole.png'
            im.save(whole)
            # 4x2 mosaic in row-major order, like NASA's A1..D2 BMNG tiles.
            parts = []
            side = width // 4
            for row in range(2):
                for column in range(4):
                    part = root/f'part-{row}-{column}.png'
                    im.crop((column*side, row*side, (column+1)*side, (row+1)*side)).save(part)
                    parts.append(part)
            for positive, left in (('east', 180), ('west', 0)):
                outputs = {}
                for name, source, mosaic, backend in (
                        ('pil', whole, (1, 1), 'pil'),
                        ('vips', whole, (1, 1), 'vips'),
                        ('mosaic', parts, (4, 2), 'auto')):
                    out = root/f'{name}-{positive}'
                    build(source, out, 256, 4, positive, left, mosaic=mosaic,
                          backend=backend, workers=3)
                    outputs[name] = out
                reference = outputs['pil']
                names = sorted(f.name for f in reference.iterdir())
                self.assertIn('tile-l1-004-008.jpg', names)
                for name in ('vips', 'mosaic'):
                    self.assertEqual(sorted(f.name for f in outputs[name].iterdir()), names)
                    self.assertEqual((outputs[name]/'manifest.txt').read_text(),
                                     (reference/'manifest.txt').read_text())
                    # Level 0 is a pure copy: identical source pixels give
                    # identical JPEGs. Coarser levels and previews may differ
                    # by resampler rounding only.
                    for f in names:
                        if not f.endswith('.jpg'):
                            continue
                        with Image.open(reference/f) as a, Image.open(outputs[name]/f) as b:
                            extrema = ImageChops.difference(a.convert('RGB'),
                                                            b.convert('RGB')).getextrema()
                        limit = 0 if f.startswith('tile-0') else 40
                        self.assertLessEqual(max(hi for _, hi in extrema), limit,
                                             f'{name} {positive} {f}')

    def test_mosaic_shape_is_checked(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            part = root/'part.png'
            Image.new('RGB', (64, 64)).save(part)
            with self.assertRaises(ValueError):
                build([part, part, part], root/'out', 64, 4, mosaic=(2, 2))


if __name__ == '__main__':
    unittest.main()
