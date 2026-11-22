#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert headerless signed 16-bit metre elevations into GLES2-friendly tiles.

Input: row-major, north-to-south, west-to-east, little-endian int16 values.
Accepts 2:1 cell-registered or (2N+1)x(N+1) node-registered world
rasters. Node grids are *averaged into geographic cell centers*, not merely
cropped, to avoid a half-pixel displacement at the coastline.

Requires numpy and Pillow ONLY at asset build time. No runtime dependency.
"""
import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image

# Signed metres, RGB.  Separate land and water stops around sea level.
# The output's legend.txt contains this exact table for the application's UI.
STOPS = (
    (-11000, (8, 18, 46)),
    (-8000, (10, 42, 86)),
    (-6000, (20, 72, 123)),
    (-4000, (37, 112, 166)),
    (-2000, (62, 155, 194)),
    (-200, (124, 198, 211)),
    (-1, (178, 227, 228)),
    (0, (50, 116, 73)),
    (200, (113, 156, 75)),
    (1000, (193, 184, 107)),
    (2500, (166, 140, 112)),
    (4500, (198, 185, 165)),
    (6500, (239, 232, 218)),
    (9000, (255, 255, 255)),
)


def colorize(data):
    """Return RGB for elevations (metres), with a visible missing-data color."""
    out = np.empty((*data.shape, 3), dtype=np.uint8)
    values = data.astype(np.float32, copy=False)
    xs = [stop[0] for stop in STOPS]
    for channel in range(3):
        out[..., channel] = np.interp(values, xs, [c[channel] for _, c in STOPS]).astype(np.uint8)
    out[data == -32768] = [170, 20, 170]  # explicit no-data pixels
    return out


def read_grid(source, width, height):
    if width < 2 or height < 1 or width > 100000 or width * height > 250_000_000:
        raise ValueError('invalid elevation raster dimensions (limit: 250 megapixels)')
    nodes = width == 2 * height - 1
    if not nodes and width != 2 * height:
        raise ValueError('expected a 2:1 pixel grid or a (2N+1)x(N+1) node grid')
    source = Path(source)
    if source.stat().st_size != width * height * 2:
        raise ValueError('raw elevation file size does not match signed 16-bit dimensions')
    grid = np.memmap(source, dtype='<i2', mode='r', shape=(height, width))
    out_w = width - 1 if nodes else width
    out_h = height - 1 if nodes else height
    return grid, nodes, out_w, out_h


def sample(grid, node_grid, xx, yy):
    """Sample at map-cell centers. xx and yy may be broadcastable arrays."""
    if not node_grid:
        return np.asarray(grid[yy, xx], dtype=np.int16)
    # NOAA ETOPO1 grid-registered posts lie on latitude/longitude grid lines.
    # Convert four adjacent posts to a cell-centered height in metres. Accumulate
    # in int32 to avoid overflow at the lowest ocean depths.
    a = grid[yy, xx].astype(np.int32)
    b = grid[yy, xx + 1].astype(np.int32)
    c = grid[yy + 1, xx].astype(np.int32)
    d = grid[yy + 1, xx + 1].astype(np.int32)
    # Preserve NoData rather than fabricating heights if a neighbour is invalid.
    missing = (a == -32768) | (b == -32768) | (c == -32768) | (d == -32768)
    values = np.rint((a + b + c + d) / 4.0).astype(np.int16)
    return np.where(missing, -32768, values)


def preview_image(grid, node_grid, width, height, out_width):
    out_height = out_width // 2
    # Evenly resample to pixels near each target cell center, avoiding creation
    # of a gigabyte-sized full-size RGB image for ETOPO1.
    xx = np.minimum(width - 1, ((np.arange(out_width) + 0.5) * width / out_width).astype(np.int32))
    yy = np.minimum(height - 1, ((np.arange(out_height) + 0.5) * height / out_height).astype(np.int32))
    return Image.fromarray(colorize(sample(grid, node_grid, xx[None, :], yy[:, None])), 'RGB')


def build(source, destination, width=21601, height=10801, tile_size=1024,
          gutter=4, attribution='NOAA ETOPO1 ICE SURFACE (2009)'):
    if tile_size < 64 or tile_size > 2048 or tile_size & (tile_size - 1):
        raise ValueError('tile size must be power of two in [64,2048]')
    if gutter < 2 or gutter > 16 or gutter * 2 >= tile_size:
        raise ValueError('invalid tile gutter')
    if not attribution.isascii() or any(ord(c) < 32 for c in attribution) or len(attribution) > 90:
        raise ValueError('attribution must be one line of at most 90 characters')
    grid, nodes, width, height = read_grid(source, width, height)
    content = tile_size - 2 * gutter
    cols, rows = math.ceil(width / content), math.ceil(height / content)
    if cols * rows > 8192:
        raise ValueError('too many elevation tiles')
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    for row in range(rows):
        for col in range(cols):
            left, top = col * content, row * content
            cell_w, cell_h = min(content, width-left), min(content, height-top)
            valid_w, valid_h = cell_w + 2*gutter, cell_h + 2*gutter
            xs = np.minimum(np.arange(tile_size), valid_w - 1) + left - gutter
            ys = np.minimum(np.arange(tile_size), valid_h - 1) + top - gutter
            # Longitude wraps; latitude clamps at the poles.
            xx = (xs % width).astype(np.int32)
            yy = np.clip(ys, 0, height-1).astype(np.int32)
            rgb = colorize(sample(grid, nodes, xx[None, :], yy[:, None]))
            Image.fromarray(rgb, 'RGB').save(destination / f'tile-{row:03d}-{col:03d}.jpg',
                                                    quality=93, subsampling=0)
    preview_image(grid, nodes, width, height, 2048).save(destination / 'preview.jpg', quality=90)
    preview_image(grid, nodes, width, height, 1024).save(destination / 'preview-small.jpg', quality=90)
    (destination / 'manifest.txt').write_text(
        f'GLOBTOO_RASTER_TILES 1\n{width} {height} {tile_size} {gutter} {cols} {rows}\n', encoding='ascii')
    (destination / 'legend.txt').write_text(
        'GLOBTOO_RASTER_LEGEND 1\n'
        'ELEVATION AND BATHYMETRY\n'
        'METRES\n'
        + attribution + '\n' + str(len(STOPS)) + '\n'
        + ''.join(f'{value} {r} {g} {b}\n' for value, (r, g, b) in STOPS), encoding='ascii')
    return cols, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', help='headerless signed little-endian int16 elevation, metres')
    parser.add_argument('output', help='output directory for tiled JPEGs and legend')
    parser.add_argument('--width', type=int, default=21601)
    parser.add_argument('--height', type=int, default=10801)
    parser.add_argument('--tile-size', type=int, default=1024)
    parser.add_argument('--gutter', type=int, default=4)
    parser.add_argument('--attribution', default='NOAA ETOPO1 ICE SURFACE (2009)')
    args = parser.parse_args()
    cols, rows = build(args.source, args.output, args.width, args.height,
                       args.tile_size, args.gutter, args.attribution)
    print(f'Generated {cols}x{rows} elevation/bathymetry tiles and physical-unit legend')


if __name__ == '__main__':
    main()
