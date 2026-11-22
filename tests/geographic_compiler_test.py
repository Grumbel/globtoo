# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
import importlib.util
import json
import pathlib
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("compile_geography", ROOT / "tools/build_geographic_features.py")
compiler = importlib.util.module_from_spec(spec)
spec.loader.exec_module(compiler)


class CompilerTest(unittest.TestCase):
    def test_places_rivers_and_antimeridian(self):
        with tempfile.TemporaryDirectory() as directory:
            p = pathlib.Path(directory)
            places = {"type": "FeatureCollection", "features": [
                {"type": "Feature", "geometry": {"type": "Point", "coordinates": [13.405, 52.52]},
                 "properties": {"NAMEASCII": "Berlin", "ADM0CAP": 1, "POP_MAX": 3770000, "SCALERANK": 1}},
                {"type": "Feature", "geometry": {"type": "Point", "coordinates": [8.68, 50.11]},
                 "properties": {"NAMEASCII": "Frankfurt", "SCALERANK": 6}},
                {"type": "Feature", "geometry": {"type": "Point", "coordinates": [400, 2]},
                 "properties": {"NAME": "bad"}},
            ]}
            rivers = {"type": "FeatureCollection", "features": [
                {"type": "Feature", "geometry": {"type": "MultiLineString", "coordinates": [
                    [[179.5, 0], [-179.5, 0]], [[-10, 2], [-9.5, 2.1]]]},
                 "properties": {"SCALERANK": 2}},
                {"type": "Feature", "geometry": {"type": "LineString", "coordinates": [[0,0],[3,3]]},
                 "properties": {"SCALERANK": 15}},
            ]}
            (p / 'cities.geojson').write_text(json.dumps(places), encoding='utf-8')
            (p / 'rivers.geojson').write_text(json.dumps(rivers), encoding='utf-8')
            cities = compiler.compile_places(p / 'cities.geojson')
            lines = compiler.compile_rivers(p / 'rivers.geojson')
            self.assertEqual(len(cities),2)
            self.assertEqual(cities[0][5],b'Berlin')
            self.assertTrue(len(lines) >= 4)
            self.assertTrue(all(line[2]==2 for line in lines))
            self.assertTrue(all(.9 < sum(x*x for x in line[0]) < 1.1 for line in lines))
            output = p / 'data.gfeat'
            compiler.write_binary(output,cities,lines)
            self.assertEqual(output.read_bytes()[:8],b'GLBFEAT1')
            self.assertEqual(len(output.read_bytes()) > 100,True)


if __name__ == '__main__':
    unittest.main()
