#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Prepare a full equirectangular JPEG as power-of-two GLES2 tile assets.

Runs at build time, not at application startup.  Small cyclic/clamped gutters
keep bilinear filtering and mipmaps continuous at tile borders and the dateline.
"""
import argparse
import math
from pathlib import Path
from PIL import Image

# NASA's 21600x10800 global image is 233 million pixels, above Pillow's
# default decompression-bomb warning/error threshold. This known use case is
# intentional; impose our own finite input limit before decompressing.
Image.MAX_IMAGE_PIXELS = 250_000_000


def build(source, destination, tile_size=1024, gutter=4):
    if tile_size & (tile_size - 1) or tile_size < 64 or tile_size > 2048:
        raise ValueError("tile size must be a power of two in [64, 2048]")
    if not 2 <= gutter <= 16 or gutter * 2 >= tile_size:
        raise ValueError("invalid tile gutter")
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    with Image.open(source) as image:
        image = image.convert('RGB')
        width, height = image.size
        if width * height > 250_000_000:
            raise ValueError("raster exceeds the 250-megapixel build limit")
        if width != 2 * height:
            raise ValueError("expected a north-up, full-world 2:1 equirectangular raster")
        content = tile_size - 2 * gutter
        columns, rows = math.ceil(width / content), math.ceil(height / content)
        if columns * rows > 8192:
            raise ValueError("too many raster tiles")
        # Every tile is tile_size², including small final edge cells, making
        # GL_LINEAR_MIPMAP_LINEAR legal even on strict GLES2 NPOT hardware.
        for row in range(rows):
            for column in range(columns):
                left, top = column * content, row * content
                cell_w = min(content, width - left)
                cell_h = min(content, height - top)
                tile = Image.new('RGB', (tile_size, tile_size))
                # Copy contiguous strips, wrapping at the longitude seam.
                # All operations are Pillow block copies (no Python per-pixel
                # loops, even for a 21600x10800 NASA image).
                x_begin, x_end = left - gutter, left + cell_w + gutter
                y_begin, y_end = top - gutter, top + cell_h + gutter
                y0, y1 = max(0, y_begin), min(height, y_end)
                for wrap in (-width, 0, width):
                    xa, xb = max(x_begin, wrap), min(x_end, wrap + width)
                    if xa < xb and y0 < y1:
                        part = image.crop((xa-wrap, y0, xb-wrap, y1))
                        tile.paste(part, (xa-x_begin, y0-y_begin))
                valid_w, valid_h = cell_w+2*gutter, cell_h+2*gutter
                if y_begin < 0:
                    edge = tile.crop((0, -y_begin, valid_w, -y_begin+1))
                    tile.paste(edge.resize((valid_w, -y_begin)), (0, 0))
                if y_end > height:
                    from_y = height - y_begin
                    edge = tile.crop((0, from_y-1, valid_w, from_y))
                    tile.paste(edge.resize((valid_w, valid_h-from_y)), (0, from_y))
                # Pad incomplete last row/column with the closest real texel.
                valid_w = cell_w + 2*gutter
                valid_h = cell_h + 2*gutter
                if valid_w < tile_size:
                    edge = tile.crop((valid_w-1, 0, valid_w, valid_h))
                    tile.paste(edge.resize((tile_size-valid_w, valid_h)), (valid_w, 0))
                if valid_h < tile_size:
                    edge = tile.crop((0, valid_h-1, tile_size, valid_h))
                    tile.paste(edge.resize((tile_size, tile_size-valid_h)), (0, valid_h))
                tile.save(destination / f'tile-{row:03d}-{column:03d}.jpg',
                          quality=93, subsampling=0)
        # Use a small preview for zoomed-out views; loading its full resolution
        # cannot exceed GLES2's ubiquitous 2048x2048 limit.
        preview = image.resize((2048, 1024), Image.Resampling.LANCZOS)
        preview.save(destination / 'preview.jpg', quality=90)
        preview_small = image.resize((1024, 512), Image.Resampling.LANCZOS)
        preview_small.save(destination / 'preview-small.jpg', quality=90)
        (destination / 'manifest.txt').write_text(
            f'GLOBTOO_RASTER_TILES 1\n{width} {height} {tile_size} {gutter} {columns} {rows}\n',
            encoding='ascii')
    return columns, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source')
    ap.add_argument('output')
    ap.add_argument('--tile-size', type=int, default=1024)
    ap.add_argument('--gutter', type=int, default=4)
    args = ap.parse_args()
    print('Created %d x %d raster tiles' % build(args.source, args.output,
                                                  args.tile_size, args.gutter))


if __name__ == '__main__':
    main()
