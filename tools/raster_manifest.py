# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared definition of the GLOBTOO_RASTER_TILES manifest and tile names.

Version 1 is a single full-resolution tile grid.  Version 2 adds a mip-like
pyramid: level 0 is the full-resolution grid (same file names as version 1),
each coarser level halves the height (rounding up) and keeps an exact 2:1
width.  All levels share the tile size and gutter, and live in the same flat
directory, so hashing, copying and installation treat them uniformly.
Must stay in sync with src/tiled_raster.cpp.
"""
from dataclasses import dataclass
import re

MAGIC = 'GLOBTOO_RASTER_TILES'
MAX_LEVELS = 8
MAX_TILES = 8192
# Widest accepted source: the 109164-pixel LROC WAC 100 m Moon mosaic.
MAX_WIDTH = 131072
# A coarser level is only worth storing while it is still sharper than the
# 2048-pixel preview that the runtime always keeps resident.
PREVIEW_WIDTH = 2048
TILE_RE = re.compile(r'tile-(?:l[1-7]-)?\d{3}-\d{3}\.jpg\Z')


@dataclass(frozen=True)
class RasterLayout:
    width: int
    height: int
    tile_size: int
    gutter: int
    columns: int
    rows: int
    levels: int = 1

    @property
    def content(self) -> int:
        return self.tile_size - 2 * self.gutter

    def level_size(self, level: int) -> tuple[int, int]:
        height = self.height
        for _ in range(level):
            height = (height + 1) // 2
        return 2 * height, height

    def level_grid(self, level: int) -> tuple[int, int]:
        width, height = self.level_size(level)
        return (-(-width // self.content), -(-height // self.content))

    def tile_count(self) -> int:
        return sum(c * r for c, r in map(self.level_grid, range(self.levels)))

    def expected_tiles(self) -> set[str]:
        names = set()
        for level in range(self.levels):
            columns, rows = self.level_grid(level)
            names.update(tile_name(level, r, c)
                         for r in range(rows) for c in range(columns))
        return names


def pyramid_levels(width: int, height: int) -> int:
    """Number of levels for a full-resolution width x height source."""
    levels = 1
    layout = RasterLayout(width, height, 64, 2, 1, 1, 1)
    while levels < MAX_LEVELS and layout.level_size(levels)[0] > PREVIEW_WIDTH:
        levels += 1
    return levels


def tile_name(level: int, row: int, column: int) -> str:
    prefix = 'tile-' if level == 0 else f'tile-l{level}-'
    return f'{prefix}{row:03d}-{column:03d}.jpg'


def format_manifest(layout: RasterLayout) -> str:
    base = (f'{layout.width} {layout.height} {layout.tile_size} {layout.gutter} '
            f'{layout.columns} {layout.rows}')
    if layout.levels == 1:
        return f'{MAGIC} 1\n{base}\n'
    return f'{MAGIC} 2\n{base} {layout.levels}\n'


def parse_manifest(text: str) -> RasterLayout:
    words = text.split()
    if len(words) < 2 or words[0] != MAGIC or words[1] not in ('1', '2'):
        raise ValueError(f'invalid tile manifest header (expected {MAGIC} 1 or 2)')
    expected_words = 8 if words[1] == '1' else 9
    if len(words) != expected_words:
        raise ValueError('invalid tile manifest field count')
    try:
        values = list(map(int, words[2:]))
    except ValueError as exc:
        raise ValueError('invalid tile manifest integers') from exc
    layout = RasterLayout(*values) if len(values) == 7 else RasterLayout(*values, 1)
    content = layout.content
    if not (2 <= layout.width <= MAX_WIDTH and layout.height >= 1 and
            layout.width == 2 * layout.height and
            64 <= layout.tile_size <= 2048 and
            layout.tile_size & (layout.tile_size - 1) == 0 and
            2 <= layout.gutter <= 16 and content > 0 and
            1 <= layout.columns <= 256 and 1 <= layout.rows <= 256 and
            layout.columns == -(-layout.width // content) and
            layout.rows == -(-layout.height // content) and
            (words[1] == '1' or 2 <= layout.levels <= MAX_LEVELS) and
            layout.tile_count() <= MAX_TILES):
        raise ValueError('impossible tile layout')
    return layout
