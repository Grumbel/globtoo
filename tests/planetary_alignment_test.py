#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Headless alignment-report tests; no network or scientific raster required."""

import json
import math
from pathlib import Path
import tempfile
import unittest

from PIL import Image, ImageDraw
from tools.planetary_alignment import (CATALOG, pixel_center, read_features,
                                       render, report_image)
from tools.normalize_planet_texture import normalize
from tools.build_raster_tiles import build


class PlanetaryAlignmentTest(unittest.TestCase):
    def test_every_solid_body_has_landmarks(self):
        from tools.planetary_bodies import BODY_INFO
        for body, info in BODY_INFO.items():
            if body == 'earth' or info['surface'] != 'solid':
                with self.assertRaisesRegex(ValueError, 'no features'):
                    read_features(CATALOG, body)
            else:
                self.assertIn(len(read_features(CATALOG, body)), (2, 3), body)
        mercury = {f['name']: f for f in read_features(CATALOG, 'mercury')}
        self.assertAlmostEqual(mercury['Caloris Planitia']['longitude_east'], 170.2)

    def test_feature_catalog_and_longitudes(self):
        self.assertEqual([len(read_features(CATALOG, body)) for body in ('moon', 'mars', 'io')],
                         [3, 3, 3])
        io = {f['name']: f for f in read_features(CATALOG, 'io')}
        self.assertAlmostEqual(io['Loki Patera']['longitude_east'], 51.2)
        self.assertAlmostEqual(io['Pele']['longitude_east'], 104.72)
        self.assertAlmostEqual(io['Tvashtar Paterae']['longitude_east'], -123.53)
        mars = {f['name']: f for f in read_features(CATALOG, 'mars')}
        self.assertAlmostEqual(mars['Olympus Mons']['longitude_east'], -134)
        self.assertEqual(mars['Olympus Mons']['latitude_type'], 'planetocentric')
        with self.assertRaisesRegex(ValueError, 'no features'):
            read_features(CATALOG, 'earth')

    def test_canonical_pixel_centers_and_seams(self):
        self.assertEqual(pixel_center(0, 0, 720, 360), (359.5, 179.5))
        self.assertEqual(pixel_center(-180, 0, 720, 360),
                         pixel_center(180, 0, 720, 360))
        self.assertEqual(pixel_center(540, 0, 720, 360),
                         pixel_center(180, 0, 720, 360))
        self.assertEqual(pixel_center(0, 90, 720, 360)[1], 0)
        self.assertEqual(pixel_center(0, -90, 720, 360)[1], 359)
        with self.assertRaisesRegex(ValueError, '2:1'):
            pixel_center(0, 0, 700, 400)
        with self.assertRaisesRegex(ValueError, 'latitude'):
            pixel_center(0, 91, 720, 360)

    def test_annotations_match_normalized_pixels(self):
        width, height = 720, 360
        canonical = Image.new('RGB', (width, height), '#222222')
        draw = ImageDraw.Draw(canonical)
        io = read_features(CATALOG, 'io')
        for f in io:
            x, y = pixel_center(f['longitude_east'], f['latitude'], width, height)
            draw.ellipse((round(x)-3, round(y)-3, round(x)+3, round(y)+3),
                         fill=(255, 240, 240))
        # The west-positive source is an inverse transform of the canonical
        # viewer image. Normalizing it must restore the landmark positions.
        raw = normalize(canonical, 'west', 0, width)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            src, output, report = root/'io.png', root/'annotated.png', root/'report.json'
            raw.save(src)
            positioned = render(src, output, body='io', positive='west', left=0,
                                width=width, report=report)
            self.assertEqual([f['name'] for f in positioned],
                             ['Loki Patera', 'Pele', 'Tvashtar Paterae'])
            details = json.loads(report.read_text())
            self.assertEqual(details['image_width'], width)
            self.assertEqual(details['longitude_positive'], 'west')
            self.assertEqual(details['features'][0]['gazetteer_id'], 3459)
            sheet = Image.open(output)
            self.assertEqual(sheet.size, (width, height+32))
            # Spot-check that red crosshair is drawn where the source's
            # canonical white dot lies, not at a mirrored longitude.
            for feature in positioned:
                x, y = round(feature['x']), round(feature['y'])
                self.assertGreater(sheet.getpixel((x,y+32))[0], 100)
                self.assertLess(math.dist((x,y), pixel_center(feature['longitude_east'],
                                                     feature['latitude'],width,height)), 1)

    def test_normalized_tiles_and_mars_warp(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root/'mars.png'
            im = Image.new('RGB', (720,360))
            for y in range(360):
                for x in range(720):
                    im.putpixel((x,y),(x%256, y%256, ((x*3+y*2)//5)%256))
            im.save(source)
            tiles=root/'tiles'
            build(source,tiles,128,4,'west',0,'planetographic','mars')
            position = render(tiles,root/'mars-alignment.png', body='mars',
                              normalized=True,width=1024)
            self.assertEqual(len(position),3)
            self.assertTrue((root/'mars-alignment.png').exists())
            with self.assertRaisesRegex(ValueError, 'cannot be combined'):
                render(tiles,root/'bad.png',body='mars',normalized=True,
                       positive='west')
            with self.assertRaisesRegex(ValueError, 'explicit'):
                render(source,root/'bad.png',body='mars')
            with self.assertRaisesRegex(ValueError, 'already normalized'):
                render(tiles,root/'bad.png',body='mars',positive='east',left=180)

    def test_giant_planet_sheet_has_grid_only(self):
        # Cloud-top maps have no landmarks, but a prepared map still needs
        # its QA sheet and report; they must not fail or invent features.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            src, report = root/'jupiter.png', root/'jupiter.json'
            Image.new('RGB', (256, 128), (180, 140, 100)).save(src)
            positioned = render(src, root/'jupiter-qa.png', body='jupiter',
                                normalized=True, width=256, report=report)
            self.assertEqual(positioned, [])
            self.assertEqual(json.loads(report.read_text())['features'], [])
            self.assertTrue((root/'jupiter-qa.png').exists())

    def test_invalid_dimensions(self):
        with self.assertRaisesRegex(ValueError, '2:1'):
            report_image(Image.new('RGB',(100,100)),'moon',[],'test')
        with tempfile.TemporaryDirectory() as temporary:
            src=Path(temporary)/'bad.png'
            Image.new('RGB',(300,300)).save(src)
            with self.assertRaisesRegex(ValueError, '2:1'):
                render(src,Path(temporary)/'out.png',body='moon',normalized=True)
            with self.assertRaisesRegex(ValueError, 'different files'):
                render(src,src,body='moon',normalized=True)
            with self.assertRaisesRegex(ValueError, 'JSON report must not overwrite'):
                render(src,Path(temporary)/'out.png',body='moon',normalized=True,
                       report=src)
            with self.assertRaisesRegex(ValueError, 'filename'):
                render(src,Path(temporary)/'out.jpg',body='moon',normalized=True)


if __name__ == '__main__':
    unittest.main()
