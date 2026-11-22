#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Normalize a full-globe planetary plate carrée raster to Globtoo's UV convention.

The source must cover 360 degrees by 180 degrees, north at its top, with
uniform angular sampling. Longitude at its LEFT PIXEL BOUNDARY is either
0 degrees or 180 degrees in the declared positive longitude direction.
The result has east-positive longitude, -180 degrees at its left boundary.

Source latitude is planetocentric by default. For Mars, --body mars
--latitude planetographic resamples geodetic latitude onto the centric sphere.
Moon and Io use spherical IAU/USGS Gazetteer reference coordinates.
"""

import argparse
from pathlib import Path
import sys

try:
    from PIL import Image, ImageOps
except ImportError as error:
    raise SystemExit("Pillow is required to normalize planetary raster maps") from error

if __package__:
    from .planetary_latitudes import reproject_latitude_rows, validate_latitude_type
else:
    from planetary_latitudes import reproject_latitude_rows, validate_latitude_type


def normalize(image: Image.Image, positive: str, left: int, max_width: int,
              latitude: str = "planetocentric", body: str | None = None) -> Image.Image:
    validate_latitude_type(body, latitude)
    width, height = image.size
    if width < 2 or width % 2 or width != 2 * height:
        raise ValueError("expected an even-width full-globe 2:1 plate carree image")
    if max_width < 2 or max_width % 2:
        raise ValueError("--max-width must be an even number >= 2")
    if positive not in {"east", "west"} or left not in {0, 180}:
        raise ValueError("longitude direction must be east/west; left boundary must be 0/180")

    # Reduce resolution in the source frame first. This also avoids loading
    # the full-resolution product as a GLES2 texture at runtime. The caller
    # must use a suitable tiled pipeline if 2048 pixels is insufficient.
    image = image.convert("RGB")
    if width > max_width:
        image = image.resize((max_width, max_width // 2), Image.Resampling.LANCZOS)
        width = max_width

    # West-positive maps run opposite the viewer's east-positive X axis.
    # Mirror maps the longitudinal sampling *pixel centers* exactly (no
    # one-pixel antimeridian error). The half-turn is lossless for 0..360
    # rasters; no bilinear sampling, clipped poles or duplicate seam column.
    if positive == "west":
        image = ImageOps.mirror(image)
    if left == 0:
        half = width // 2
        # Rotate the periodic sequence by half a revolution.
        image = _rotate_half(image, half)
    return reproject_latitude_rows(image, body, latitude)


def _rotate_half(image: Image.Image, half: int) -> Image.Image:
    result = Image.new("RGB", image.size)
    result.paste(image.crop((half, 0, image.width, image.height)), (0, 0))
    result.paste(image.crop((0, 0, half, image.height)), (half, 0))
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path, help=".jpg or .png normalized output")
    parser.add_argument("--positive", required=True, choices=("east", "west"),
                        help="positive longitude direction in source raster")
    parser.add_argument("--left", required=True, type=int, choices=(0, 180),
                        help="longitude at source image's left boundary (0 or 180 degrees)")
    parser.add_argument("--max-width", type=int, default=2048,
                        help="downsample to this even width for GLES2 (default 2048)")
    parser.add_argument("--latitude", choices=("planetocentric", "planetographic"),
                        default="planetocentric", help="source latitude convention")
    parser.add_argument("--body", choices=("moon", "mars", "io"),
                        help="required for planetographic input; only Mars needs resampling")
    args = parser.parse_args(argv)
    if args.source.resolve() == args.output.resolve():
        parser.error("source and destination must differ")
    if args.output.suffix.lower() not in (".jpg", ".jpeg", ".png"):
        parser.error("output must be JPEG or PNG")
    try:
        with Image.open(args.source) as src:
            result = normalize(src, args.positive, args.left, args.max_width,
                               args.latitude, args.body)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        if args.output.suffix.lower() in (".jpg", ".jpeg"):
            result.save(args.output, format="JPEG", quality=93, subsampling=0)
        else:
            result.save(args.output, format="PNG")
    except (ValueError, OSError) as error:
        print(f"planet texture: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
