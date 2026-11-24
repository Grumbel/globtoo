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
from PIL import Image, ImageOps

if __package__:
    from .planetary_bodies import BODIES
    from .raster_manifest import (MAX_TILES, RasterLayout, format_manifest,
                                  pyramid_levels, tile_name)
    from .planetary_latitudes import (row_indices, reproject_latitude_rows,
                                      validate_latitude_type)
else:
    from planetary_bodies import BODIES
    from raster_manifest import (MAX_TILES, RasterLayout, format_manifest,
                                 pyramid_levels, tile_name)
    from planetary_latitudes import (row_indices, reproject_latitude_rows,
                                     validate_latitude_type)

# NASA's 21600x10800 global image is 233 million pixels, above Pillow's
# default decompression-bomb warning/error threshold. This known use case is
# intentional; impose our own finite input limit before decompressing.
Image.MAX_IMAGE_PIXELS = 250_000_000


def _oriented_strip(image, tile, x_begin, x_end, y0, y1, y_begin,
                    positive, left, top='north'):
    """Paste source strips in canonical -180..180 east-positive pixel order.

    Only crop strips needed by a tile. Mirroring or half-turning a 21600x10800
    mosaic in its entirety would require another ~700 MiB of memory.
    Pixel-center transforms match normalize_planet_texture.normalize exactly.
    """
    width = image.width
    # Rows y0..y1 are canonical (north-up); south-up sources store them
    # bottom-to-top, so read the mirrored row range and flip the strip.
    src_y0, src_y1 = (image.height - y1, image.height - y0) if top == 'south' else (y0, y1)
    half = width // 2 if left == 0 else 0
    reverse = positive == 'west'

    def source_x(output_x):
        x = (output_x + half) % width
        return width - 1 - x if reverse else x

    x = x_begin
    while x < x_end:
        start = x
        first = source_x(x)
        last = first
        x += 1
        step = -1 if reverse else 1
        while x < x_end and source_x(x) == last + step:
            last = source_x(x)
            x += 1
        patch = image.crop((min(first, last), src_y0, max(first, last)+1, src_y1))
        if reverse:
            patch = ImageOps.mirror(patch)
        if top == 'south':
            patch = ImageOps.flip(patch)
        tile.paste(patch, (start - x_begin, y0 - y_begin))


def _oriented_preview(image, positive, left, top='north'):
    # Apply exactly the same ordering as the full-resolution strips, but only
    # after reducing the preview to 2048x1024 pixels.
    if top == 'south':
        image = ImageOps.flip(image)
    if positive == 'west':
        image = ImageOps.mirror(image)
    if left == 0:
        half = image.width // 2
        result = Image.new('RGB', image.size)
        result.paste(image.crop((half, 0, image.width, image.height)), (0, 0))
        result.paste(image.crop((0, 0, half, image.height)), (half, 0))
        image = result
    return image


def _write_level(image, destination, level, tile_size, gutter, positive,
                 left, latitude, body, top_edge):
    """Tile one pyramid level; `image` is the (resized) unoriented source."""
    width, height = image.size
    content = tile_size - 2 * gutter
    columns, rows = math.ceil(width / content), math.ceil(height / content)
    # Every tile is tile_size², including small final edge cells, making
    # GL_LINEAR_MIPMAP_LINEAR legal even on strict GLES2 NPOT hardware.
    for row in range(rows):
        for column in range(columns):
            tile_left, top = column * content, row * content
            cell_w = min(content, width - tile_left)
            cell_h = min(content, height - top)
            tile = Image.new('RGB', (tile_size, tile_size))
            # Copy contiguous strips, wrapping at the longitude seam.
            # All operations are Pillow block copies (no Python per-pixel
            # loops, even for a 21600x10800 NASA image).
            x_begin, x_end = tile_left - gutter, tile_left + cell_w + gutter
            y_begin, y_end = top - gutter, top + cell_h + gutter
            y0, y1 = max(0, y_begin), min(height, y_end)
            if latitude == 'planetographic' and body == 'mars':
                # Geodetic Mars maps have nonlinear Y spacing relative
                # to the viewer's centric sphere. Sample each canonical
                # output scanline from at most two source scanlines, never
                # allocating a second full-resolution mosaic. Longitude
                # reorientation still uses the seam-aware strip copier.
                valid_w, valid_h = cell_w + 2*gutter, cell_h + 2*gutter
                for dst_y in range(valid_h):
                    a, b, fraction = row_indices(y_begin + dst_y, height,
                                                 body, latitude)
                    first = Image.new('RGB', (valid_w, 1))
                    _oriented_strip(image, first, x_begin, x_end,
                                    a, a+1, a, positive, left, top_edge)
                    if b != a and fraction > 0:
                        second = Image.new('RGB', (valid_w, 1))
                        _oriented_strip(image, second, x_begin, x_end,
                                        b, b+1, b, positive, left, top_edge)
                        first = Image.blend(first, second, fraction)
                    tile.paste(first, (0, dst_y))
            elif y0 < y1:
                _oriented_strip(image, tile, x_begin, x_end, y0, y1,
                                y_begin, positive, left, top_edge)
            valid_w, valid_h = cell_w+2*gutter, cell_h+2*gutter
            if y_begin < 0 and not (latitude == 'planetographic' and body == 'mars'):
                edge = tile.crop((0, -y_begin, valid_w, -y_begin+1))
                tile.paste(edge.resize((valid_w, -y_begin)), (0, 0))
            if y_end > height and not (latitude == 'planetographic' and body == 'mars'):
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
            tile.save(destination / tile_name(level, row, column),
                      quality=93, subsampling=0)


def build(source, destination, tile_size=1024, gutter=4,
          positive='east', left=180, latitude='planetocentric', body=None,
          top_edge='north'):
    validate_latitude_type(body, latitude)
    if top_edge not in ('north', 'south'):
        raise ValueError('top edge must be north or south')
    if positive not in ('east', 'west') or left not in (0, 180):
        raise ValueError('longitude must be east|west with left boundary 0|180')
    if tile_size & (tile_size - 1) or tile_size < 64 or tile_size > 2048:
        raise ValueError("tile size must be a power of two in [64, 2048]")
    if not 2 <= gutter <= 16 or gutter * 2 >= tile_size:
        raise ValueError("invalid tile gutter")
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    with Image.open(source) as image:
        width, height = image.size
        if width * height > 250_000_000:
            raise ValueError("raster exceeds the 250-megapixel build limit")
        if width != 2 * height:
            raise ValueError("expected a full-world 2:1 equirectangular raster")
        image = image.convert('RGB')
        content = tile_size - 2 * gutter
        columns, rows = math.ceil(width / content), math.ceil(height / content)
        layout = RasterLayout(width, height, tile_size, gutter, columns, rows,
                              pyramid_levels(width, height))
        if layout.tile_count() > MAX_TILES:
            raise ValueError("too many raster tiles")
        # Level 0 is full resolution. Coarser levels are resized from the
        # source *before* reorientation: mirroring and the half-turn shift
        # commute with resampling because every level width is even.
        for level in range(layout.levels):
            level_image = image if level == 0 else image.resize(
                layout.level_size(level), Image.Resampling.LANCZOS)
            _write_level(level_image, destination, level, tile_size, gutter,
                         positive, left, latitude, body, top_edge)
            del level_image
        # Use a small preview for zoomed-out views; loading its full resolution
        # cannot exceed GLES2's ubiquitous 2048x2048 limit.
        preview = reproject_latitude_rows(_oriented_preview(
            image.resize((2048, 1024), Image.Resampling.LANCZOS), positive, left, top_edge),
            body, latitude)
        preview.save(destination / 'preview.jpg', quality=90)
        preview_small = reproject_latitude_rows(_oriented_preview(
            image.resize((1024, 512), Image.Resampling.LANCZOS), positive, left, top_edge),
            body, latitude)
        preview_small.save(destination / 'preview-small.jpg', quality=90)
        (destination / 'manifest.txt').write_text(format_manifest(layout),
                                                  encoding='ascii')
    return columns, rows


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('source')
    ap.add_argument('output')
    ap.add_argument('--tile-size', type=int, default=1024)
    ap.add_argument('--gutter', type=int, default=4)
    ap.add_argument('--positive', choices=('east','west'), default='east',
                    help='source longitude-positive direction (default east)')
    ap.add_argument('--left', type=int, choices=(0, 180), default=180,
                    help='source left longitude boundary (default 180)')
    ap.add_argument('--top', choices=('north', 'south'), default='north',
                    help='pole at the source top edge (default north)')
    ap.add_argument('--latitude', choices=('planetocentric', 'planetographic'),
                    default='planetocentric', help='source latitude convention')
    ap.add_argument('--body', choices=BODIES,
                    help='required for planetographic input; only Mars needs resampling')
    args = ap.parse_args()
    if args.latitude == 'planetographic' and args.body is None:
        ap.error('planetographic latitude requires --body')
    print('Created %d x %d raster tiles' % build(args.source, args.output,
                                                  args.tile_size, args.gutter,
                                                  args.positive, args.left,
                                                  args.latitude, args.body,
                                                  args.top))


if __name__ == '__main__':
    main()
