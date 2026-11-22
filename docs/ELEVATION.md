<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Measured elevation and bathymetry (Stage 3b)

Globtoo offers **three distinct Earth surface raster modes**:

- **Imagery:** the usual Earth photograph, shipped with the default Nix package.
- **Relief:** an optional *colorized picture* of terrain and seafloor. Its colors
  do **not** encode exact depth or elevation; it has no numerical legend.
- **Elevation (ELEV):** an optional numerical model of elevation above and depth
  below mean sea level. The map is colorized from source measurements in **metres**
  and displays a signed, color-matched scale with provenance. Negative is below
  sea level, zero is sea level, positive is elevation above sea level.

Countries and the selected raster remain independently enabled, and the
translucent political overlay blends with the thematic image as before. ELEV
works on the 3D sphere, map projections, and their folding/unfolding animation.
The meter scale is a *height/depth legend*, not a statement that the map uses a
metric horizontal projection; the separately displayed kilometer ruler still
measures horizontal surface distance.

## Source and preparation

The converter accepts **headerless, signed 16-bit little-endian**, row-major
values in **whole metres**, north to south, starting at 180°W. NOAA's
[ETOPO1 global relief model](https://www.ncei.noaa.gov/products/etopo-global-relief-model)
provides such data in its `*_i2` binary distribution. The *Ice Surface* model
shows the upper surface of Greenland/Antarctic ice, while *Bedrock* shows the
bedrock beneath those ice sheets. The distinction matters in polar views.

NOAA documents the file layout in its
[ETOPO1 binary README](https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO1/data/ice_surface/grid_registered/binary/readme_etopo1_binary.txt).
For the grid-registered Ice Surface file, for example, use
`etopo1_ice_g_i2.bin` (21601 × 10801, 466,624,802 bytes). It includes the
extra 180°E meridian and the south-pole row. The converter **averages four
adjacent height posts at each output cell center**, producing a geographic
21,600 × 10,800 image. This avoids the half-cell shift that simply dropping
one row and column would cause. NOAA also supplies the directly cell-registered
21600 × 10800 variant, which the same converter accepts.

Download and unpack the chosen binary from NOAA yourself; these large data
files are neither tracked in Git nor downloaded at runtime.

```sh
# Python 3, NumPy and Pillow required only for conversion
python3 tools/build_elevation_tiles.py \
  /data/etopo1_ice_g_i2.bin /tmp/globtoo-elevation

# Launch with the local prepared raster; includes a metres legend
# and source/year information (2009 ETOPO1 dataset).
globtoo --layer elevation --elevation-dir /tmp/globtoo-elevation
```

`--width` and `--height` permit other worldwide i16 grids with the same
coordinate/orientation and units. Use `--attribution 'YOUR SOURCE (YEAR)'` for
non-ETOPO1 data; do not leave the NOAA attribution on an unrelated dataset.
`-32768` is reserved for NoData and appears magenta. Other signed values are
clamped to the end colors at the legend's -11000 m to +9000 m scale.

To build and install the tiles together with the application:

```sh
cmake -S . -B build \
  -DGLOBTOO_EARTH_TEXTURE=/path/to/earth.jpg \
  -DGLOBTOO_ELEVATION_RAW=/data/etopo1_ice_g_i2.bin
cmake --build build -j
cmake --install build --prefix "$HOME/.local"
```

`GLOBTOO_ELEVATION_WIDTH` and `GLOBTOO_ELEVATION_HEIGHT` override the source
size (defaults are ETOPO1's 21601 × 10801). CMake only needs NumPy and Pillow
when the optional numeric input is configured. The output includes 1024px
JPEG tiles with dateline gutters, previews, `manifest.txt`, and `legend.txt`.
At runtime it reuses the same bounded lazy-loading cache as the visual relief
layer. Build/install output is under `share/globtoo/elevation`.

**Current limitations:** We tested synthetic signed-height rasters and a
small build-time fixture, not a full ETOPO1 distribution. The JPEGs visualize
numerical source values but are not a lossless height store; the legend describes
the mapping, not a pixel-query API. All input must already be in a global,
north-up, ±180° equirectangular geographic grid. No GeoTIFF/netCDF/GDAL
coordinate reprojection, raw-data height query, per-pixel exact readout,
terrain displacement, or automatic external download is implemented.

The source product is public NOAA data; the converter code is
GPL-3.0-or-later. Users remain responsible for verifying attribution, byte
order, spatial registration and year of any alternate data they supply.
