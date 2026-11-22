<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Moon, Mars and Io: planetary surface maps

Globtoo models all bodies with a spherical, planetocentric-style viewer. In
its **internal** convention, longitude is positive **east**, latitude is
positive **north**, the left border of a full-world raster is **180°W**, and
the right border is **180°E**. The top row is north. The sphere, labels,
projection modes, and markers use that same convention. Body radius changes
the on-screen great-circle scale; it does **not** establish a precise datum.

**External mosaics have their own cartographic metadata.** USGS and PDS
products may use east- or west-positive longitude, 0–360° or ±180° domains,
planetocentric or planetographic latitudes, and regional coverage. Direction
and domain **cannot** be inferred from the name "Mars" or "Io" alone.

## Named planetary features

The bundled **small** feature catalog contains nine IAU-approved names and
feature-center coordinates from the [USGS Gazetteer of Planetary Nomenclature](https://planetarynames.wr.usgs.gov/). These are reference point labels,
not outlines and not a complete scientific GIS dataset. Click **LANDMARKS**
(on Moon, Mars or Io) or press **F** to toggle them. Use
`globtoo --list-features` to see the names, or start at one with
`globtoo --focus 'Olympus Mons'`. This switches to the correct body and
centers the camera. The labels work without any external texture.

| Body | Label | Source center coordinates | Gazetteer entry |
| --- | --- | --- | --- |
| Moon | Tycho | 43.30°S, 11.22°W | [6163](https://planetarynames.wr.usgs.gov/Feature/6163) |
| Moon | Copernicus | 9.62°N, 20.08°W | [1296](https://planetarynames.wr.usgs.gov/Feature/1296) |
| Moon | Mare Tranquillitatis | 8.35°N, 30.83°E | [3691](https://planetarynames.wr.usgs.gov/Feature/3691) |
| Mars | Olympus Mons | 18.40°N, 226.00°E | [4453](https://planetarynames.wr.usgs.gov/Feature/4453) |
| Mars | Gale | 5.44°S, 137.70°E | [2071](https://planetarynames.wr.usgs.gov/Feature/2071) |
| Mars | Hellas Planitia | 42.66°S, 70.00°E | [2432](https://planetarynames.wr.usgs.gov/Feature/2432) |
| Io | Loki Patera | 12.97°N, 308.80°W | [3459](https://planetarynames.wr.usgs.gov/Feature/3459) |
| Io | Pele | 18.71°S, 255.28°W | [4638](https://planetarynames.wr.usgs.gov/Feature/4638) |
| Io | Tvashtar Paterae | 62.76°N, 123.53°W | [14131](https://planetarynames.wr.usgs.gov/Feature/14131) |

**Coordinates are not interchangeable between bodies.** The selected lunar
entries are planetographic latitude, east-positive longitude in −180–180;
the selected Martian entries are planetocentric latitude, east-positive
longitude in 0–360; the Io entries are planetographic latitude and
**west-positive** longitude in 0–360. Globtoo flips Io's longitude sign and
wraps all source longitudes to [−180°, 180°) east-positive, but the spherical
renderer does **not** transform planetographic latitude to planetocentric
latitude. Feature positions on the Moon and Io are therefore **approximate**;
errors from actual mosaic datum/projection and shape modeling have not been
quantified. This catalog is deliberately separate from source-texture
longitude normalization. The IAU feature center is not guaranteed to equal
a feature's visually brightest or highest point.

## Preparing an externally obtained map

The input must be a color/grayscale **full 360° × 180° north-up plate carrée**
image, with exactly 2:1 dimensions (uniform longitude and latitude spacing),
no added map borders or legend. TIFF/GeoTIFF support depends on Pillow's
installed codecs; other supported raster formats include PNG and JPEG. The
script **does not reproject** Mercator, polar stereographic, regional mosaics,
planetographic latitude, or an arbitrary geographic TIFF. Inspect the input's
original map metadata and crop/reproject with appropriate GIS software first.

Normalize an east-positive map whose left boundary is at longitude 0°:

```sh
python3 tools/normalize_planet_texture.py moon_source.tif moon.jpg \
  --positive east --left 0 --max-width 2048
```

Normalize a west-positive map whose left boundary is at 0°W:

```sh
python3 tools/normalize_planet_texture.py io_source.tif io.jpg \
  --positive west --left 0 --max-width 2048
```

For ±180° maps, use `--left 180`, regardless of positive direction. The
preprocessor mirrors west-positive pixels and rotates 0–360° images by half
a revolution; its operations have exact pixel-center alignment, including
the antimeridian seam and both poles. Downsampling occurs before rearranging
the pixels. The maximum width defaults to 2048 to avoid exceeding typical
GLES2 texture limits; the runtime further checks `GL_MAX_TEXTURE_SIZE`.

Run with `globtoo --body moon --body-texture moon=/path/to/moon.jpg`, or install
one of the normalized maps as `share/globtoo/assets/{moon,mars,io}.jpg`.
Globtoo discovers installed planetary maps when switching bodies, while an
explicit `--body-texture` overrides the installed file. The application does
not download any textures without user action.

## High-resolution tiled Moon, Mars and Io imagery

For full-world mosaics wider than a GLES2 texture limit, use the **same
power-of-two, lazily uploaded tile pipeline** as Earth imagery instead of
reducing the whole map to 2048 pixels. The tile preparer understands the
source longitude convention directly: it mirrors/rotates **per tile** and
normalizes the overview without allocating a second full-size mosaic.

For example, given a locally obtained **planetocentric, north-up, 2:1,
full-world** Io color mosaic that has west-positive longitudes and a 0°W
left boundary (verify these claims against the actual product metadata):

```sh
python3 tools/build_raster_tiles.py /data/io-source.tif /data/io-tiles \
  --positive west --left 0

globtoo --body io --body-imagery-dir io=/data/io-tiles
```

`--body-imagery-dir moon=DIR`, `mars=DIR`, and `earth=DIR` can be repeated,
so the `M` key and NEXT BODY button retain separate tile sources. The legacy
`--imagery-dir DIR` remains the Earth shortcut. An explicit source directory
must contain `manifest.txt`, `preview.jpg`, and `tile-NNN-NNN.jpg` files.
For normal startup and mid-projection morphs the small preview is used; at
close zoom, tiles are loaded lazily with the existing bounded GPU cache. The
application never accesses the network while drawing or changing bodies.

CMake can also package the mosaics under
`share/globtoo/imagery/{moon,mars,io}/` for automatic detection:

```sh
cmake -S . -B build \
  -DGLOBTOO_IO_IMAGERY_TEXTURE=/data/io-source.tif \
  -DGLOBTOO_IO_LONGITUDE_POSITIVE=west \
  -DGLOBTOO_IO_LONGITUDE_LEFT=0 \
  -DGLOBTOO_IO_LATITUDE_TYPE=planetocentric \
  '-DGLOBTOO_IO_SOURCE=Exact product identifier and URL' \
  '-DGLOBTOO_IO_LICENSE=Verified redistribution terms'
cmake --build build
cmake --install build
```

Equivalent `GLOBTOO_MOON_IMAGERY_TEXTURE` and
`GLOBTOO_MARS_IMAGERY_TEXTURE` inputs are available, including when a small
per-body `*_TEXTURE` is not provided. The installed tiles include a
`source.txt` record of the supplied provenance and original longitude
convention. CMake rejects missing source metadata, unsupported longitude
orientation or absent input files. The default packages do **not** bundle
unverified source mosaics.

This is **longitude-axis normalization**, not a general cartographic
reprojection. Planetographic latitudes, global maps with displaced seams,
regional swaths, positive-south latitudes, 360° endpoint duplication and
non-equirectangular projections need conversion with planetary-aware GIS
tools before tiling. The input's width must equal exactly twice its height.

## CMake packaging with provenance

The build can prepare and install each planetary texture directly from its
explicitly supplied source. The source, redistribution terms, positive
longitude direction and longitude at the **left image boundary** are required.
For example, for a verified, full-world east-positive 0–360° lunar mosaic:

```sh
cmake -S . -B build \
  -DGLOBTOO_MOON_TEXTURE=/data/lunar_mosaic.tif \
  -DGLOBTOO_MOON_LONGITUDE_POSITIVE=east \
  -DGLOBTOO_MOON_LONGITUDE_LEFT=0 \
  -DGLOBTOO_MOON_LATITUDE_TYPE=planetocentric \
  -DGLOBTOO_MOON_SOURCE='USGS Astrogeology: [exact product identifier/URL]' \
  -DGLOBTOO_MOON_LICENSE='[verified source license]' \
  -DGLOBTOO_PLANET_MAX_WIDTH=2048
cmake --build build
cmake --install build
```

Replace bracketed fields with **actual verified** provenance and terms. The
same options exist for `GLOBTOO_MARS_*` and `GLOBTOO_IO_*`. The normalized
JPEG and its `<body>.source.txt` attribution are staged beside the binary
and included in the installed resource directory. Input imagery is not
stored in the Git repository. Leave these CMake options empty to keep the
existing smaller package without planetary mosaics.

## Promising data catalogs (not bundled or verified downloads)

- **Moon:** [USGS LRO LROC WAC Global Morphology Mosaic](https://astrogeology.usgs.gov/search/map/moon_lro_lroc_wac_global_morphology_mosaic_100m).
- **Mars:** [USGS Viking Colorized Global Mosaic](https://astrogeology.usgs.gov/search/map/mars_viking_colorized_global_mosaic_232m); its published cylindrical files use east longitude and planetocentric latitude.
- **Io:** [USGS Galileo SSI/Voyager Color Merged Global Mosaic](https://astrogeology.usgs.gov/search/map/io_galileo_ssi_voyager_color_merged_global_mosaic_1km); some published Io maps use west-positive longitudes, so check the **specific dataset**.
- [USGS planetary coordinate/projection guidance](https://astrogeology.usgs.gov/docs/concepts/camera-geometry-and-projections/learning-about-map-projections/) and [PDS longitude-direction metadata](https://pds.nasa.gov/datastandards/documents/dd/current/PDS4_PDS_DD_1J00/webhelp/all/ch07s45.html).

These are **catalog references**, not confirmed rights-cleared, fetched,
reprojected, or checksum-pinned assets. In particular, a page's small preview
is not necessarily the original scientific raster, and TIFF georeferencing
must be inspected before normalization. Do not add any product to the default
package until its bytes, source license, datum, row orientation, and actual
longitude domain have been verified; pin a real cryptographic digest in any
subsequent Nix fetch.

## Limitations

- The rendering model uses a **sphere of mean radius**, not an ellipsoid or
  detailed shape model. It reports approximate surface distances, not
  elevation-aware paths.
- Longitude mirroring/rotation does **not** correct planetographic vs.
  planetocentric latitude or shifted scientific prime meridians.
- Planetary high-resolution tiling and a small sourced landmark catalog exist.
  A complete gazetteer import and body-specific terrain relief remain future
  work. Existing measured elevation, population, cities, rivers and country
  boundaries are Earth-only.
- Hardware rendering and the geographic alignment of actual mosaics need
  testing with the original verified datasets, not just synthetic fixtures.
