#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Render a visual registration QA sheet for a planetary plate carrée mosaic.

Source mode applies exactly the same longitude and optional Mars latitude
normalization as Globtoo's texture preparer before plotting IAU feature centers.
Tiles mode uses a previously normalized raster-tile preview as-is.

This tool is deliberately NOT a scientific geolocation validator: a centered
Gazetteer label is not ground truth for the rim, peak, or full outline, and
source-map prime meridians and control networks must still be checked.
"""

import argparse
import csv
import json
from pathlib import Path
import sys

from PIL import Image, ImageDraw, ImageFont

if __package__:
    from .normalize_planet_texture import normalize
else:
    from normalize_planet_texture import normalize

CATALOG = Path(__file__).resolve().parents[1] / 'data' / 'planetary_features.tsv'


def read_features(filename: Path, body: str) -> list[dict]:
    rows: list[dict] = []
    with filename.open(encoding='utf-8', newline='') as stream:
        reader = csv.reader((line for line in stream if line.strip() and not line.startswith('#')),
                            delimiter='\t')
        for fields in reader:
            if len(fields) != 8:
                raise ValueError('planetary feature record needs 8 tab-separated fields')
            row_body, name, kind, lat, lon, direction, lat_type, gazetteer_id = fields
            if row_body != body:
                continue
            if direction not in ('east', 'west') or lat_type not in ('planetographic', 'planetocentric'):
                raise ValueError(f'unsupported feature coordinate convention: {name}')
            latitude, longitude = float(lat), float(lon)
            if not (-90 <= latitude <= 90 and -180 <= longitude <= 360):
                raise ValueError(f'invalid feature coordinates: {name}')
            east = (longitude if direction == 'east' else -longitude)
            east = (east + 180) % 360 - 180
            rows.append(dict(name=name, kind=kind, latitude=latitude,
                             longitude_east=east, gazetteer_id=int(gazetteer_id),
                             source_longitude=longitude, source_direction=direction,
                             latitude_type=lat_type))
    if not rows:
        raise ValueError(f'no features for body: {body}')
    return rows


def pixel_center(longitude_east: float, latitude: float, width: int, height: int) -> tuple[float, float]:
    """Return a continuous pixel-center location on a canonical full-world map."""
    if width != 2 * height or width < 2:
        raise ValueError('expected an even-width 2:1 raster')
    if not (-90 <= latitude <= 90):
        raise ValueError('latitude must be within [-90, 90]')
    x = min(width - 1, max(0, ((longitude_east + 180) % 360) / 360 * width - 0.5))
    y = min(height - 1, max(0, (90 - latitude) / 180 * height - 0.5))
    return x, y


def report_image(image: Image.Image, body: str, features: list[dict], title: str,
                 grid_degrees: int = 30) -> tuple[Image.Image, list[dict]]:
    """Draw circles and lat/lon grid at exact canonical pixel-center positions."""
    if grid_degrees not in (10, 15, 30, 45, 90):
        raise ValueError('grid step must divide 90 degrees')
    image = image.convert('RGB').copy()
    w, h = image.size
    if w != 2 * h or w < 2:
        raise ValueError('expected a 2:1 raster')
    draw = ImageDraw.Draw(image, 'RGBA')
    font = ImageFont.load_default()
    for lon in range(-180, 180, grid_degrees):
        x, _ = pixel_center(lon, 0, w, h)
        draw.line([(round(x), 0), (round(x), h - 1)], fill=(20, 240, 240, 125), width=1)
    for lat in range(-90 + grid_degrees, 90, grid_degrees):
        _, y = pixel_center(0, lat, w, h)
        draw.line([(0, round(y)), (w - 1, round(y))], fill=(20, 240, 240, 125), width=1)

    info = []
    for feature in features:
        x, y = pixel_center(feature['longitude_east'], feature['latitude'], w, h)
        ix, iy = round(x), round(y)
        radius = max(5, w // 350)
        draw.ellipse((ix-radius, iy-radius, ix+radius, iy+radius),
                     outline=(255, 40, 40, 255), width=2)
        draw.line((ix-2*radius, iy, ix+2*radius, iy), fill=(255, 245, 40, 255), width=2)
        draw.line((ix, iy-2*radius, ix, iy+2*radius), fill=(255, 245, 40, 255), width=2)
        label = feature['name']
        text_x = min(w - draw.textbbox((0, 0), label, font=font)[2] - 3,
                     max(3, ix+2*radius+4))
        text_y = max(3, min(h-16, iy-14))
        draw.text((text_x, text_y), label, font=font, fill=(255,255,255,255),
                  stroke_width=2, stroke_fill=(0,0,0,255))
        info.append(dict(feature, x=x, y=y))
    # Header is external to the raster: the plotted coordinates are not moved.
    header_height = 32
    sheet = Image.new('RGB', (w, h + header_height), '#111c29')
    sheet.paste(image, (0, header_height))
    ImageDraw.Draw(sheet).text((8, 9), title, font=font, fill='#f8f8f8')
    return sheet, info


def render(source: Path, output: Path, *, body: str,
           normalized: bool = False, positive: str | None = None, left: int | None = None,
           latitude: str = 'planetocentric', width: int = 2048,
           report: Path | None = None, catalog: Path = CATALOG) -> list[dict]:
    if width < 2 or width % 2 or width > 8192:
        raise ValueError('--width must be even, between 2 and 8192')
    if normalized:
        if positive is not None or left is not None or latitude != 'planetocentric':
            raise ValueError('--normalized cannot be combined with source coordinate flags')
        path = source / 'preview.jpg' if source.is_dir() else source
        if source.is_dir() and not (source / 'manifest.txt').is_file():
            raise ValueError('tile directory requires a manifest.txt')
    else:
        if source.is_dir():
            raise ValueError('tile directories are already normalized: use --normalized')
        if positive not in ('east', 'west') or left not in (0, 180):
            raise ValueError('source requires explicit --positive and --left metadata')
        path = source

    if output.suffix.lower() != '.png':
        raise ValueError('QA sheet output must have a .png filename')
    if output.resolve() == path.resolve():
        raise ValueError('source and QA output must be different files')
    if report is not None and report.resolve() in (path.resolve(), output.resolve()):
        raise ValueError('JSON report must not overwrite the source or QA PNG')

    with Image.open(path) as loaded:
        if normalized:
            if loaded.width != 2 * loaded.height:
                raise ValueError('normalized preview must be 2:1')
            raster = loaded.convert('RGB')
            if raster.width > width:
                raster = raster.resize((width, width // 2), Image.Resampling.LANCZOS)
        else:
            raster = normalize(loaded, positive, left, width, latitude, body)

    features = read_features(catalog, body)
    sheet, positioned = report_image(raster, body, features,
                                     f'{body.upper()}  /  Gazetteer center registration (visual QA only)')
    output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(output, format='PNG')
    if report is not None:
        report.parent.mkdir(parents=True, exist_ok=True)
        report.write_text(json.dumps({
            'body': body, 'input': str(source), 'normalized': normalized,
            'longitude_positive': positive, 'longitude_left': left,
            'latitude_type': latitude if not normalized else 'planetocentric',
            'image_width': raster.width, 'image_height': raster.height,
            'coordinates': 'east-positive/planetocentric; x,y refer to raster BEFORE the 32px header',
            'features': positioned,
            'caution': 'Gazetteer centers are approximate; visual checks cannot validate map datum or control network.'
        }, indent=2) + '\n', encoding='utf-8')
    return positioned


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path, help='original full-world image, or normalized image / tile directory')
    parser.add_argument('output', type=Path, help='annotated PNG registration sheet')
    parser.add_argument('--body', choices=('moon', 'mars', 'io'), required=True)
    parser.add_argument('--positive', choices=('east', 'west'))
    parser.add_argument('--left', type=int, choices=(0, 180))
    parser.add_argument('--latitude', choices=('planetocentric', 'planetographic'),
                        default='planetocentric')
    parser.add_argument('--normalized', action='store_true',
                        help='input already normalized, or an existing tiled output directory')
    parser.add_argument('--width', type=int, default=2048, help='maximum QA raster width')
    parser.add_argument('--json', type=Path, help='also write machine-readable pixel coordinates')
    args = parser.parse_args()
    try:
        features = render(args.input, args.output, body=args.body,
                          normalized=args.normalized, positive=args.positive,
                          left=args.left, latitude=args.latitude, width=args.width,
                          report=args.json)
    except (OSError, ValueError) as exc:
        parser.exit(1, f'planetary alignment: {exc}\n')
    for feature in features:
        print(f"{feature['name']}: lon(E+) {feature['longitude_east']:.2f} "
              f"lat(N+) {feature['latitude']:.2f} -> "
              f"x={feature['x']:.2f} y={feature['y']:.2f}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
