# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Convert raster latitude sampling to Globtoo's planetocentric sphere.

The USGS/IAU Gazetteer models the Moon and Io as spheres for cartographic
nomenclature, so their planetographic latitude is *already* planetocentric
on that reference surface. Mars has a biaxial reference ellipsoid. For Mars
  tan(phi_c) = (polar_radius / equatorial_radius)^2 * tan(phi_g)
where phi_g is geodetic/planetographic and phi_c is planetocentric.

Only latitude is resampled, at *pixel centers*. This is not a general map
projection or datum/control-network conversion. Input must already be a
north-up, full-globe, equally spaced plate carree raster.

Sources:
https://planetarynames.wr.usgs.gov/Page/Website
https://planetarynames.wr.usgs.gov/Page/MARS/system
https://nssdc.gsfc.nasa.gov/planetary/factsheet/marsfact.html
"""
import math

from PIL import Image


# NASA Mars Fact Sheet equatorial and polar radii (km); these are reference
# ellipsoid parameters, NOT radii used for Globtoo's spherical distance ruler.
MARS_EQUATORIAL_KM = 3396.2
MARS_POLAR_KM = 3376.2
BODIES = ('moon', 'mars', 'io')
LATITUDE_TYPES = ('planetocentric', 'planetographic')


def validate_latitude_type(body, latitude):
    if latitude not in LATITUDE_TYPES:
        raise ValueError('latitude must be planetocentric or planetographic')
    if body is not None and body not in BODIES:
        raise ValueError('body must be moon, mars or io')
    if latitude == 'planetographic' and body is None:
        raise ValueError('planetographic latitude needs --body moon|mars|io')


def source_row(output_y, height, body, latitude):
    """Subpixel source row for the center of a canonical planetocentric row."""
    if latitude != 'planetographic' or body != 'mars':
        return float(max(0, min(height - 1, output_y)))
    lat_c = math.radians(90.0 - (output_y + 0.5) * 180.0 / height)
    ratio = (MARS_EQUATORIAL_KM / MARS_POLAR_KM) ** 2
    lat_g = math.atan2(ratio * math.sin(lat_c), math.cos(lat_c))
    y = (90.0 - math.degrees(lat_g)) * height / 180.0 - 0.5
    return max(0.0, min(height - 1.0, y))


def row_indices(output_y, height, body, latitude):
    y = source_row(output_y, height, body, latitude)
    before = math.floor(y)
    after = min(height - 1, before + 1)
    return before, after, y - before


def reproject_latitude_rows(image, body, latitude):
    """Reproject a *small* image, intended for the GLES2 single/preview map.

    For full-resolution mosaics, build_raster_tiles uses row_indices to load
    only individual tile strips; do not use this on a 21600x10800 image.
    """
    validate_latitude_type(body, latitude)
    if latitude != 'planetographic' or body != 'mars':
        return image
    out = Image.new('RGB', image.size)
    for y in range(image.height):
        a, b, fraction = row_indices(y, image.height, body, latitude)
        upper = image.crop((0, a, image.width, a + 1))
        if b != a and fraction > 0:
            lower = image.crop((0, b, image.width, b + 1))
            upper = Image.blend(upper, lower, fraction)
        out.paste(upper, (0, y))
    return out
