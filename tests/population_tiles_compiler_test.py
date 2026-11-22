# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic data tests: never label persons per pixel as persons/km2."""
import pathlib
import tempfile
import unittest

import numpy as np
from PIL import Image
from tools.build_population_tiles import (PopulationGrid, build, colorize,
                                           NO_DATA, STOPS)


class PopulationTest(unittest.TestCase):
    def test_measured_density_and_dateline_gutters(self):
        with tempfile.TemporaryDirectory() as d:
            base = pathlib.Path(d)
            arr = np.zeros((32, 64), dtype='<f4')
            arr[:, 0] = 1
            arr[:, -1] = 20000
            arr[:, 17] = 10000
            arr[:, 40] = np.nan
            arr[2, 2] = -9999
            source = base / 'density.raw'
            arr.tofile(source)
            self.assertEqual(build(source, base/'out', year=2020,
                                   attribution='TEST MEASUREMENTS',
                                   width=64, height=32, tile_size=64), (2,1))
            legend = (base/'out'/'legend.txt').read_text()
            self.assertIn('POPULATION DENSITY 2020', legend)
            self.assertIn('PEOPLE / KM2', legend)
            self.assertIn('TEST MEASUREMENTS (2020, 20250 ARCSEC)', legend)
            self.assertIn('LOG1P',legend)
            self.assertIn(str(STOPS[-1][0]),legend)
            with Image.open(base/'out'/'preview.jpg') as img:
                self.assertEqual(img.size,(2048,1024))
            # The gutter wraps the last column to the left of -180.
            with Image.open(base/'out'/'tile-000-000.jpg') as img:
                left=img.getpixel((3,10))
                right=img.getpixel((4,10))
            self.assertNotEqual(left, right)
            self.assertTrue((base/'out'/'tile-000-001.jpg').exists())
            self.assertEqual(colorize(np.array([[float('nan'),-5]],dtype=np.float32))[0,0].tolist(),list(NO_DATA))
            self.assertTrue(np.all(colorize(np.array([[10,20000]],dtype=np.float32))[0,0] !=
                                   colorize(np.array([[10,20000]],dtype=np.float32))[0,1]))
            with self.assertRaisesRegex(ValueError,'year'):
                build(source,base/'bad',year=0,attribution='TEST',width=64,height=32)
            with self.assertRaisesRegex(ValueError,'size'):
                PopulationGrid(source,66,33)
            with self.assertRaisesRegex(ValueError,'attribution'):
                build(source,base/'bad',year=2020,attribution='',width=64,height=32)

    def test_geotiff_georeferencing_and_nodata(self):
        try:
            import rasterio
            from rasterio.transform import from_origin
        except ImportError:
            self.skipTest('rasterio optional')
        with tempfile.TemporaryDirectory() as d:
            root=pathlib.Path(d)
            raw=np.zeros((32,64),dtype=np.float32)
            raw[:,0]=6000; raw[:,-1]=100
            raw[5,7]=-9999
            path=root/'density.tif'
            with rasterio.open(path,'w',driver='GTiff',width=64,height=32,count=1,
                               dtype='float32',crs='EPSG:4326',
                               transform=from_origin(-180,90,360/64,180/32),
                               nodata=-9999) as dst:
                dst.write(raw,1)
            grid=PopulationGrid(path)
            try:
                values=grid.sample(np.array([-1,0,7,63,64]),np.array([5]))
                self.assertEqual(values.shape,(1,5))
                self.assertEqual(values[0,0],100)
                self.assertEqual(values[0,1],6000)
                self.assertEqual(values[0,2],-9999)
                self.assertEqual(values[0,4],6000)
                self.assertEqual(grid.nodata,-9999)
            finally:
                grid.close()
            self.assertEqual(build(path,root/'tiles',year=2020,
                                   attribution='GPWV4 R11',tile_size=64),(2,1))
            # Reject cropped/non-world or upside-down data.
            bad=root/'partial.tif'
            with rasterio.open(bad,'w',driver='GTiff',width=64,height=32,count=1,
                               dtype='float32',crs='EPSG:4326',
                               transform=from_origin(-179,90,360/64,180/32)) as dst:
                dst.write(raw,1)
            with self.assertRaisesRegex(ValueError,'global north-up'):
                PopulationGrid(bad)


if __name__ == '__main__':
    unittest.main()
