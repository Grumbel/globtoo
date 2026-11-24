#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Resample a nearly-2:1 global plate carrée raster onto an exact 2:1 grid.

Several published planetary mosaics are a pixel or two away from an exact
360x180 degree cell grid (for example 1225x613, or 3600x1802 with a
duplicated pole row). The raster compiler requires exact 2:1 sampling, so
this build step rescales the width to twice the height. It refuses any
source whose aspect deviates by more than --tolerance, which would mean the
raster is not a full globe and must not be silently stretched.

The expected source size is pinned by the caller; a changed upstream file
then fails loudly instead of being quietly re-gridded. Output is always an
RGB JPEG (grayscale and palette sources are expanded).
"""
import argparse
from pathlib import Path
import sys

from PIL import Image

Image.MAX_IMAGE_PIXELS = 250_000_000


def fit(image: Image.Image, tolerance: float = 0.01) -> Image.Image:
    width, height = image.size
    if width < 2 or height < 1:
        raise ValueError('empty raster')
    if abs(width / (2 * height) - 1) > tolerance:
        raise ValueError(f'{width}x{height} is not a full-globe 2:1 raster '
                         f'within {tolerance:.1%}')
    image = image.convert('RGB')
    if image.size != (2 * height, height):
        image = image.resize((2 * height, height), Image.Resampling.LANCZOS)
    return image


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--expect', required=True, metavar='WxH',
                        help='pinned source dimensions, e.g. 1225x613')
    parser.add_argument('--tolerance', type=float, default=0.01,
                        help='maximum relative aspect deviation (default 0.01)')
    args = parser.parse_args(argv)
    try:
        expected = tuple(int(v) for v in args.expect.lower().split('x'))
        if len(expected) != 2:
            raise ValueError
    except ValueError:
        parser.error('--expect must be WIDTHxHEIGHT')
    try:
        with Image.open(args.source) as source:
            if source.size != expected:
                raise ValueError(f'unexpected source size {source.size[0]}x{source.size[1]}, '
                                 f'pinned {args.expect}')
            result = fit(source, args.tolerance)
        result.save(args.output, format='JPEG', quality=95, subsampling=0)
    except (OSError, ValueError) as error:
        print(f'fit plate carree: {error}', file=sys.stderr)
        return 1
    print(f'{args.source.name}: {expected[0]}x{expected[1]} -> {result.width}x{result.height}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
