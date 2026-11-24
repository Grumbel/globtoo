#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exact 2:1 re-gridding of near-global planetary mosaics in the Nix build."""
import pathlib
import tempfile
import unittest

from PIL import Image

from tools.fit_plate_carree import fit, main


class FitTest(unittest.TestCase):
    def test_near_two_to_one_grayscale(self):
        result = fit(Image.new('L', (1225, 613), 77))
        self.assertEqual((result.mode, result.size), ('RGB', (1226, 613)))
        self.assertEqual(result.getpixel((600, 300)), (77, 77, 77))
        exact = fit(Image.new('RGB', (64, 32), (1, 2, 3)))
        self.assertEqual(exact.size, (64, 32))

    def test_reject_partial_globe(self):
        with self.assertRaisesRegex(ValueError, 'not a full-globe'):
            fit(Image.new('RGB', (300, 200)))

    def test_pinned_dimensions(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            Image.new('P', (201, 100)).save(root / 'in.png')
            self.assertEqual(main([str(root / 'in.png'), str(root / 'out.jpg'),
                                   '--expect', '200x100']), 1)
            self.assertEqual(main([str(root / 'in.png'), str(root / 'out.jpg'),
                                   '--expect', '201x100']), 0)
            with Image.open(root / 'out.jpg') as out:
                self.assertEqual((out.format, out.size), ('JPEG', (200, 100)))


if __name__ == '__main__':
    unittest.main()
