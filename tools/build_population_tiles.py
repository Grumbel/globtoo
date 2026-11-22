#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Make colorized GLES2 tiles from measured population density (people/km2).

Input: north-up, full-world EPSG:4326 GeoTIFF or headerless, row-major,
west-to-east/north-to-south little-endian float32 cell centers (2:1).
GeoTIFF requires rasterio, raw input needs just numpy and Pillow. Source
and year must be supplied explicitly: never guess the vintage of a raster.
"""
import argparse
import math
from pathlib import Path

import numpy as np
from PIL import Image

# Smooth heatmap with deliberately nonuniform density stops; interpolation
# occurs in log1p(persons/km2), not in population count per pixel.
STOPS = (
    (0, (240, 239, 226)),
    (1, (224, 233, 176)),
    (10, (166, 213, 140)),
    (50, (82, 185, 131)),
    (200, (40, 137, 117)),
    (500, (254, 204, 91)),
    (1000, (247, 145, 60)),
    (2500, (218, 81, 56)),
    (5000, (169, 45, 67)),
    (10000, (112, 30, 71)),
    (20000, (56, 26, 73)),
)
NO_DATA = (40, 64, 98)  # visual indicator, not a measurement


def colorize(values, nodata=None):
    a = np.asarray(values, dtype=np.float32)
    invalid = ~np.isfinite(a) | (a < 0)
    if nodata is not None and math.isfinite(nodata):
        invalid |= a == nodata
    clipped = np.clip(np.where(invalid, 0.0, a), 0, STOPS[-1][0])
    logv = np.log1p(clipped)
    xs = np.log1p(np.array([s[0] for s in STOPS], dtype=np.float32))
    rgb = np.empty((*a.shape, 3), dtype=np.uint8)
    for c in range(3):
        rgb[..., c] = np.interp(logv, xs, [s[1][c] for s in STOPS]).astype(np.uint8)
    rgb[invalid] = NO_DATA
    return rgb


class PopulationGrid:
    def __init__(self, path, width=None, height=None):
        self.path = Path(path)
        self.dataset = None
        self.nodata = None
        if self.path.suffix.lower() in ('.tif', '.tiff'):
            try:
                import rasterio
            except ImportError as e:
                raise ValueError('GeoTIFF inputs require rasterio at build time') from e
            ds = rasterio.open(path)
            t = ds.transform
            if (ds.count != 1 or ds.crs is None or ds.crs.to_epsg() != 4326 or
                not math.isclose(t.b, 0, abs_tol=1e-10) or
                not math.isclose(t.d, 0, abs_tol=1e-10) or
                t.a <= 0 or t.e >= 0 or
                not math.isclose(t.c, -180, abs_tol=1e-5) or
                not math.isclose(t.f, 90, abs_tol=1e-5) or
                not math.isclose(t.a * ds.width, 360, abs_tol=1e-5) or
                not math.isclose(t.e * ds.height, -180, abs_tol=1e-5)):
                ds.close()
                raise ValueError('GeoTIFF must be a global north-up EPSG:4326 raster (-180..180, 90..-90), one band')
            self.dataset = ds
            self.width, self.height = ds.width, ds.height
            self.nodata = ds.nodata
            if width is not None and width != self.width or height is not None and height != self.height:
                ds.close()
                raise ValueError('declared dimensions do not match GeoTIFF')
        else:
            if width is None or height is None:
                raise ValueError('raw float32 data requires --width and --height')
            self.width, self.height = width, height
            if self.path.stat().st_size != width * height * 4:
                raise ValueError('raw float32 file size does not match dimensions')
            self.raw = np.memmap(self.path, dtype='<f4', mode='r', shape=(height, width))
        if self.width < 2 or self.height < 1 or self.width != 2 * self.height or self.width > 100000 or self.width * self.height > 1_000_000_000:
            self.close()
            raise ValueError('expected a global 2:1 cell-centered raster (max 1 gigapixel)')

    def close(self):
        if self.dataset is not None:
            self.dataset.close()
            self.dataset = None

    def sample(self, xs, ys):
        """Read sparse or contiguous columns/rows, wrapping at dateline.

        GeoTIFF blocks are only read for the narrow x range needed, even for
        tiles that straddle ±180°, never for the entire world width.
        """
        xx = np.asarray(xs, dtype=np.int64) % self.width
        yy = np.clip(np.asarray(ys, dtype=np.int64), 0, self.height - 1)
        if self.dataset is None:
            return np.asarray(self.raw[yy[:, None], xx[None, :]])
        from rasterio.windows import Window
        out = np.empty((len(yy), len(xx)), dtype=np.float32)
        start = 0
        y0 = int(yy.min())
        y1 = int(yy.max()) + 1
        while start < len(xx):
            end = start + 1
            while end < len(xx) and xx[end] == xx[end-1]+1:
                end += 1
            x0 = int(xx[start]); count = int(xx[end-1]-x0+1)
            block = self.dataset.read(1, window=Window(x0, y0, count, y1-y0))
            out[:, start:end] = block[yy-y0, :]
            start = end
        return out

    def preview(self, width):
        height = width // 2
        xx = np.minimum(self.width-1, ((np.arange(width)+0.5)*self.width/width).astype(np.int32))
        yy = np.minimum(self.height-1, ((np.arange(height)+0.5)*self.height/height).astype(np.int32))
        if self.dataset is not None:
            from rasterio.enums import Resampling
            data = self.dataset.read(1, out_shape=(height, width), resampling=Resampling.nearest)
        else:
            data = self.sample(xx, yy)
        return Image.fromarray(colorize(data, self.nodata), 'RGB')


def build(source, output, *, year, attribution, width=None, height=None,
          tile_size=1024, gutter=4):
    if not isinstance(year, int) or not 1800 <= year <= 2100:
        raise ValueError('population source year must be in 1800..2100')
    if not attribution or len(attribution) > 48 or not attribution.isascii() or not all(32 <= ord(c) < 127 for c in attribution):
        raise ValueError('source attribution must be 1..48 printable ASCII characters')
    if tile_size < 64 or tile_size > 2048 or (tile_size & (tile_size - 1)):
        raise ValueError('tile size must be a power of two between 64 and 2048')
    if gutter < 2 or gutter > 16 or gutter * 2 >= tile_size:
        raise ValueError('invalid gutter')
    grid = PopulationGrid(source, width, height)
    try:
        content = tile_size - 2 * gutter
        cols = math.ceil(grid.width / content)
        rows = math.ceil(grid.height / content)
        if cols * rows > 8192:
            raise ValueError('too many raster tiles')
        output = Path(output)
        output.mkdir(parents=True, exist_ok=True)
        for row in range(rows):
            for col in range(cols):
                x, y = col * content, row * content
                cell_w = min(content, grid.width - x)
                cell_h = min(content, grid.height - y)
                xs = np.minimum(np.arange(tile_size), cell_w + 2 * gutter - 1) + x - gutter
                ys = np.minimum(np.arange(tile_size), cell_h + 2 * gutter - 1) + y - gutter
                image = Image.fromarray(colorize(grid.sample(xs, ys), grid.nodata), 'RGB')
                image.save(output / f'tile-{row:03d}-{col:03d}.jpg', quality=95, subsampling=0)
        grid.preview(2048).save(output / 'preview.jpg', quality=93, subsampling=0)
        grid.preview(1024).save(output / 'preview-small.jpg', quality=93, subsampling=0)
        (output / 'manifest.txt').write_text(
            f'GLOBTOO_RASTER_TILES 1\n{grid.width} {grid.height} {tile_size} {gutter} {cols} {rows}\n',
            encoding='ascii')
        seconds = 360 * 3600 / grid.width
        resolution = f'{seconds:g} ARCSEC'
        # v2 explicitly records the log color scale; its numeric stops retain
        # people/km2 units.  Keep the one-line attribution legible in the UI.
        attribution_line = f'{attribution} ({year}, {resolution})'
        if len(attribution_line) > 90:
            raise ValueError('source/year/resolution attribution exceeds legend limit')
        (output / 'legend.txt').write_text(
            'GLOBTOO_RASTER_LEGEND 2\n'
            f'POPULATION DENSITY {year}\nPEOPLE / KM2\n{attribution_line}\nLOG1P\n'
            + str(len(STOPS)) + '\n'
            + ''.join(f'{value} {r} {g} {b}\n' for value, (r,g,b) in STOPS),
            encoding='ascii')
        return cols, rows
    finally:
        grid.close()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', help='global EPSG:4326 GeoTIFF or raw little-endian float32 density raster')
    p.add_argument('output', help='destination for tiled JPEGs, previews, manifest, legend')
    p.add_argument('--year', type=int, required=True, help='source reference year; required to prevent mislabeling')
    p.add_argument('--attribution', required=True, help='short attribution to original dataset')
    p.add_argument('--width', type=int, help='raw float32 columns (2:1 grid)')
    p.add_argument('--height', type=int, help='raw float32 rows (2:1 grid)')
    p.add_argument('--tile-size', type=int, default=1024)
    p.add_argument('--gutter', type=int, default=4)
    a = p.parse_args()
    cols,rows = build(a.source, a.output, year=a.year, attribution=a.attribution,
                      width=a.width, height=a.height, tile_size=a.tile_size, gutter=a.gutter)
    print(f'Prepared {cols}x{rows} population density tiles ({a.year}, people/km2)')


if __name__ == '__main__':
    main()
