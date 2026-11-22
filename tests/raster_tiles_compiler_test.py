# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
import pathlib
import tempfile
import unittest
from PIL import Image
from tools.build_raster_tiles import build


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
    def test_reject_not_world(self):
        with tempfile.TemporaryDirectory() as d:
            src = pathlib.Path(d)/'bad.png'
            Image.new('RGB',(64,64)).save(src)
            with self.assertRaisesRegex(ValueError,'2:1'):
                build(src,pathlib.Path(d)/'out')

if __name__ == '__main__':
    unittest.main()
