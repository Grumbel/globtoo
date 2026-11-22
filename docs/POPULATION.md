<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Measured population density (Stage 3c)

Globtoo can display an **optional** population-density raster with a physical
legend in **people per square kilometer** (`PEOPLE / KM2`). The **POP** button
switches it on or returns to imagery. It is independent of borders, country
colors, capitals, cities, and rivers. The same tiles work on the globe, the
four flat map projections, and during morphs. Absent tiles do not affect the
ordinary Earth imagery shipped by Nix.

Unlike population *count* (people per cell), population **density** divides
population by the land area of a cell. Do **not** pass a population-count
raster or call raw pixel values persons/km2 without a matching source.

## Source and metadata

A suitable source is NASA/CIESIN SEDAC's [Gridded Population of the World v4,
Revision 11 (GPWv4)](https://data.nasa.gov/dataset/gridded-population-of-the-world-version-4-gpwv4-population-density-revision-11).
Its 2020 density product has 30 arc-second (~1 km at the equator) cells
whose values represent **people per km²**. Other years/resolutions are
published. Do not claim measurements are current: the source year represents
the modeled/estimated population vintage. Downloads may have access and
attribution conditions; check the chosen provider's terms.

Download the **population density** GeoTIFF and unpack it yourself. We do not
ship the source or silently download large raster data. The converter accepts:

- A single-band, north-up **EPSG:4326** world GeoTIFF covering 180°W to
  180°E and 90°N to 90°S. Python `rasterio` is needed at **conversion time**.
  No geographic resampling or cropping is performed, to prevent hidden
  longitude/latitude offsets. If your input is tiled by country, mosaic and
  align it onto this global grid before conversion.
- A headerless **little-endian IEEE 754 float32** raw raster, row-major,
  north-to-south and west-to-east, cell-registered and covering the full
  planet. Supply `--width` and `--height` (width = 2 × height). Negative,
  NaN, infinite and source-declared NoData values are treated as missing.
  Raw input requires only NumPy and Pillow.

```sh
python3 tools/build_population_tiles.py \
  /data/gpw_v4_density_2020.tif /tmp/globtoo-population \
  --year 2020 --attribution 'NASA SEDAC GPWV4 R11'

globtoo --layer population --population-dir /tmp/globtoo-population
```

For a prepared raw float32 raster:

```sh
python3 tools/build_population_tiles.py \
  /data/population-density-2020.f32 /tmp/globtoo-population \
  --width 43200 --height 21600 \
  --year 2020 --attribution 'NASA SEDAC GPWV4 R11'
```

**Always supply the real source and year.** These are required arguments;
Globtoo never guesses them from a filename or converts counts to density.
The output `legend.txt` records the year, angular resolution, units, and
attribution, and distinguishes a **LOG1P** color ramp from the linear
signed-metre elevation legend. The legend labels refer to the original
physical units, not arbitrary grayscale or colors. The 0–20,000 people/km²
ramp uses the logarithm of `1 + density` to resolve both rural and urban
areas; values above 20,000 are color-clamped (not changed in the original
input). Missing cells appear navy. Dark navy therefore means *no data*, not
zero population (which uses a pale color).

The converter emits JPEG tiles with dateline gutters and a lower-resolution
preview, `manifest.txt`, and `legend.txt`. JPEGs are *visualization data*;
individual RGB pixels are **not** exact numeric density queries. At runtime,
Globtoo uses the same bounded GLES2 tile cache as the Relief and ELEV layers.

## CMake installation

```sh
cmake -S . -B build \
  -DGLOBTOO_POPULATION_RASTER=/data/gpw_v4_density_2020.tif \
  -DGLOBTOO_POPULATION_YEAR=2020 \
  '-DGLOBTOO_POPULATION_ATTRIBUTION=NASA SEDAC GPWV4 R11'
cmake --build build -j
cmake --install build --prefix "$HOME/.local"
```

For raw data, additionally set `GLOBTOO_POPULATION_WIDTH` and
`GLOBTOO_POPULATION_HEIGHT`. The generated assets are installed at
`share/globtoo/population/`. No extra Python modules are required to *run*
the application. The default `nix build` deliberately does not add these
large optional data files or their source closure.

## Limitations

The input must already be in the specified geographic projection and units.
Population density is an estimate, not live census data. The optional
high-resolution GPWv4 data hasn't been checked in a full SDL3/Nix visual
session here; synthetic raw/GeoTIFF data are used for automated tests. The
legend conveys the scale of source estimates, not a queryable per-pixel
measurement interface. Verify licenses and provenance before distributing
converted assets.
