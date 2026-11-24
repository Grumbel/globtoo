#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Audit the planetary (non-Earth) raster assets in an installed Globtoo prefix.

The flake runs this *after* CMake installation.  It checks the same layout the
runtime reads: a manifest + full tile grid under imagery/<body>, and an
identical base preview under assets/<body>.jpg.  Unlike `test -s`, it detects
missing tiles, invalid JPEGs, stale manifests, and mispackaged base textures.
This is a package-integrity check, NOT proof of scientific georegistration or
publisher provenance.
"""
import argparse
from pathlib import Path
import re
import sys

from PIL import Image

if __package__:
    from .install_planetary_map import verify_prepared_map
    from .planetary_bodies import BODIES
else:
    from install_planetary_map import verify_prepared_map
    from planetary_bodies import BODIES
TILE_NAME = re.compile(r'tile-[0-9]{3}-[0-9]{3}\.jpg\Z')


def _file(path: Path) -> Path:
    if path.is_symlink() or not path.is_file() or not path.stat().st_size:
        raise ValueError(f'missing, empty, or symlinked resource: {path}')
    return path


def _jpeg(path: Path, expected: tuple[int, int] | None = None) -> tuple[int, int]:
    _file(path)
    with Image.open(path) as image:
        if image.format != 'JPEG':
            raise ValueError(f'expected JPEG: {path}')
        dimensions = image.size
        if expected is not None and dimensions != expected:
            raise ValueError(f'invalid image dimensions: {path}: {dimensions} != {expected}')
        image.load()  # Force complete decoding, not just header inspection.
    return dimensions


def verify_body(prefix: Path, body: str) -> int:
    if body not in BODIES:
        raise ValueError(f'unsupported body: {body}')
    root = prefix / 'share' / 'globtoo'
    imagery = root / 'imagery' / body
    assets = root / 'assets'
    manifest = _file(imagery / 'manifest.txt')
    if manifest.stat().st_size > 4096:
        raise ValueError(f'oversized manifest: {manifest}')
    words = manifest.read_text(encoding='ascii').split()
    if len(words) != 8 or words[:2] != ['GLOBTOO_RASTER_TILES', '1']:
        raise ValueError(f'invalid tile manifest: {manifest}')
    try:
        width, height, tile_size, gutter, cols, rows = map(int, words[2:])
    except ValueError as exc:
        raise ValueError(f'invalid tile manifest integers: {manifest}') from exc
    content = tile_size - 2 * gutter
    if not (2 <= width <= 100000 and height >= 1 and width == 2 * height and
            64 <= tile_size <= 2048 and tile_size & (tile_size - 1) == 0 and
            2 <= gutter <= 16 and content > 0 and
            1 <= cols <= 256 and 1 <= rows <= 256 and
            cols * rows <= 8192 and
            cols == (width + content - 1) // content and
            rows == (height + content - 1) // content):
        raise ValueError(f'impossible tile layout: {manifest}')

    preview_size = _jpeg(imagery / 'preview-small.jpg')
    if not (preview_size[0] == 2 * preview_size[1] and
            0 < preview_size[0] <= 2048):
        raise ValueError(f'invalid GLES2 base preview dimensions for {body}: {preview_size}')
    full_preview_size = _jpeg(imagery / 'preview.jpg')
    if not (full_preview_size[0] == 2 * full_preview_size[1] and
            0 < full_preview_size[0] <= 2048):
        raise ValueError(f'invalid preview dimensions for {body}: {full_preview_size}')
    base = _file(assets / f'{body}.jpg')
    if base.read_bytes() != (imagery / 'preview-small.jpg').read_bytes():
        raise ValueError(f'base texture does not match tiled preview: {body}')

    if (imagery / 'source.json').exists():
        # Prepared maps already have audited SHA-256 records, which must
        # survive the CMake/Nix install unchanged.
        verify_prepared_map(imagery, body)
    else:
        source = _file(imagery / 'source.txt').read_text(encoding='utf-8')
        if not all(re.search(rf'(?m)^{field}: \S', source)
                   for field in ('Source', 'License', 'Original longitude direction',
                                 'Source latitude type', 'Output')):
            raise ValueError(f'missing source or coordinate attribution for {body}')

    expected = {f'tile-{r:03d}-{c:03d}.jpg'
                for r in range(rows) for c in range(cols)}
    actual = {p.name for p in imagery.iterdir() if TILE_NAME.fullmatch(p.name)}
    if actual != expected:
        raise ValueError(f'incomplete or unexpected tile grid for {body}: '
                         f'missing={sorted(expected - actual)[:4]}, '
                         f'extra={sorted(actual - expected)[:4]}')
    for name in sorted(expected):
        _jpeg(imagery / name, (tile_size, tile_size))
    return len(expected)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('prefix', type=Path, help='installation prefix, e.g. $out')
    ap.add_argument('--body', choices=BODIES, action='append', required=True)
    args = ap.parse_args(argv)
    try:
        for body in dict.fromkeys(args.body):
            count = verify_body(args.prefix, body)
            print(f'Verified packaged {body} map: {count} readable tiles and matching base texture')
    except (ValueError, OSError, UnicodeError, ImportError, Image.DecompressionBombError) as exc:
        ap.exit(1, f'planetary package audit: {exc}\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
