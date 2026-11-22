# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
import pathlib
import tempfile
import unittest

import numpy as np
from PIL import Image
from tools.build_elevation_tiles import build, read_grid, sample, colorize, STOPS


class ElevationTest(unittest.TestCase):
    def test_raw_dimensions_and_signed_values(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            a = np.zeros((32,64), dtype='<i2')
            a[:,:32]=-8000
            a[:,32:]=3000
            a[2,5]=-32768
            (root/'heights.raw').write_bytes(a.tobytes())
            cols,rows=build(root/'heights.raw',root/'out',64,32,64,4,'TEST DATA 2026')
            self.assertEqual((cols,rows),(2,1))
            self.assertEqual(Image.open(root/'out'/'preview.jpg').size,(2048,1024))
            self.assertIn('64 32 64 4 2 1',(root/'out'/'manifest.txt').read_text())
            text=(root/'out'/'legend.txt').read_text()
            self.assertIn('TEST DATA 2026',text)
            self.assertIn('METRES',text)
            self.assertIn(str(len(STOPS)),text)
            left=Image.open(root/'out'/'tile-000-000.jpg').getpixel((12,17))
            right=Image.open(root/'out'/'tile-000-001.jpg').getpixel((8,17))
            self.assertGreater(left[2],left[0]) # ocean dark blue
            self.assertNotEqual(left,right)
            self.assertEqual(colorize(np.array([[-32768]],dtype=np.int16))[0,0].tolist(),[170,20,170])
            with self.assertRaisesRegex(ValueError,'size'):
                read_grid(root/'heights.raw',66,33)

    def test_node_registered_grid_interpolated_at_cell_center(self):
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            a=np.arange(5*9,dtype='<i2').reshape(5,9)
            (root/'nodes.raw').write_bytes(a.tobytes())
            grid,nodes,w,h=read_grid(root/'nodes.raw',9,5)
            self.assertTrue(nodes)
            self.assertEqual((w,h),(8,4))
            # first cell = average of (0,1,9,10) = 5
            self.assertEqual(int(sample(grid,nodes,np.array([[0]]),np.array([[0]]))[0,0]),5)
            self.assertEqual(build(root/'nodes.raw',root/'out',9,5,64,4),(1,1))
            self.assertIn('8 4 64 4 1 1',(root/'out'/'manifest.txt').read_text())

    def test_reject_wrong_shape_and_bad_raster(self):
        with tempfile.TemporaryDirectory() as d:
            source=pathlib.Path(d)/'bad'
            source.write_bytes(bytes(33))
            with self.assertRaises(ValueError):
                read_grid(source,8,4)
            with self.assertRaisesRegex(ValueError,'2:1'):
                read_grid(source,8,5)


if __name__ == '__main__':
    unittest.main()
