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
wraps all source longitudes to [−180°, 180°) east-positive. According to
[USGS Gazetteer coordinate guidance](https://planetarynames.wr.usgs.gov/Page/Website),
the Gazetteer uses a **spherical cartographic reference for the Moon and Io**,
so planetographic and planetocentric **latitude are numerically identical**
for these entries; their labels need no latitude correction. Mars Gazetteer
entries in our catalog already use planetocentric latitude. Differences from
an actual mosaic's control network, underlying shape and projection are still
possible and have not been quantified. This catalog is deliberately separate from source-texture
longitude normalization. The IAU feature center is not guaranteed to equal
a feature's visually brightest or highest point.

## Official raster candidates and landmark registration

These are **candidate sources**, not vetted built-in download assets. We
confirmed their published descriptions and linked image locations, but **did
not obtain or rehash the actual files in this development environment**. They
must not be added to the checked-download catalog without downloading the
bytes and recording the true SHA-256, dimensions, redistribution rights and
map projection/axis metadata. The JPL site describes the maps as full-world
textures, but does not provide detailed per-image latitude or longitude
conventions on the linked index page; do not infer those from the filename.

| Body | Original catalog page | Candidate low-res image | Notes |
| --- | --- | --- | --- |
| Moon | [NASA SVS CGI Moon Kit](https://svs.gsfc.nasa.gov/4720/) | [2025 LROC color 2K JPG](https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/lroc_color_2k.jpg) | 2048×1024; NASA says the color map is centered on 0° longitude. Polar areas use filled-in albedo; aesthetic, not a scientific base map. |
| Mars | [JPL Mars texture catalog](https://maps.jpl.nasa.gov/tmaps/mars.html) | [Viking global JPG](https://maps.jpl.nasa.gov/tmaps/pix/mar0kuu2.jpg) | Catalog lists a 1440×720 Caltech/JPL/USGS global texture. Reference system and latitude convention require checking. |
| Io | [JPL Jupiter satellite texture catalog](https://maps.jpl.nasa.gov/tmaps/jupiter.html) | [Voyager/Galileo global JPG](https://maps.jpl.nasa.gov/tmaps/pix/jup1vss2.jpg) | Catalog lists 1440×720, with color from Galileo. These older mosaics have limited polar coverage and possible control-network differences. |

For a **locally obtained** map, make a visual QA sheet without changing the
viewer or committing the bitmap. Specify metadata as determined from the source
product, **not** from the desired appearance. For example, *only if* the Moon
source is verified east-positive, centered on 0° with a −180° left edge:

```sh
python3 tools/planetary_alignment.py /data/lroc_color_2k.jpg moon-qa.png \
  --body moon --positive east --left 180 --json moon-qa.json
```

For a legacy Mars planetographic, west-positive 0°-left image, request the same
latitude and longitude conversion used by the map-preparation tools:

```sh
python3 tools/planetary_alignment.py /data/mars-source.tif mars-qa.png \
  --body mars --positive west --left 0 --latitude planetographic
```

To check already prepared tiled imagery (from any body), use its **normalized**
preview. Do **not** repeat the orientation or Mars latitude transformation:

```sh
python3 tools/planetary_alignment.py /data/io-tiles io-qa.png \
  --body io --normalized --json io-qa.json
```

The PNG includes 30° graticule lines and crosshairs/labels at the selected IAU
Gazetteer **center** coordinates. The JSON records canonical east-positive
longitude, latitude, floating pixel centers and the feature's Gazetteer ID.
The reference label catalog is in `data/planetary_features.tsv`; the C++ test
checks it against the runtime `planetary_features.hpp` table on every build.

**What to verify by eye:** Tycho and Copernicus should point to lunar craters;
Olympus Mons to the Martian shield volcano; Loki Patera and Pele to Io's
volcanic centers. Compare a few features across both hemispheres, near poles,
and on either side of the antimeridian. Horizontal mirroring, a half-turn,
and a north/south projection discrepancy have different signatures. A feature
center need not lie on the most visually distinctive part of the feature,
especially when source map resolution is low. Neither this overlay nor a
correct-looking picture proves that the source datum, reference ellipsoid,
control network, color model, georeferencing, or scientific provenance is
correct. In particular, **do not use a plausible visual match as a substitute
for the original raster metadata**.

## One-command local source preparation and audit

For a **locally downloaded** full-world map with verified source metadata,
`tools/prepare_planetary_map.py` combines the existing coordinate
normalization, GLES2 tiles, and Gazetteer QA into a single offline command.
It **does not download** a NASA/JPL map, choose its reference system, or
assert redistribution rights: supply those from the actual publisher record.
Python 3 and Pillow are required. The input remains untouched.

For example, **only if** the selected Io mosaic's original documentation
confirms west-positive longitudes with a 0°W left edge, north-up plate carrée
sampling, and spherical planetographic latitude:

```sh
python3 tools/prepare_planetary_map.py /data/io-original.jpg /data/io-tiles \
  --body io \
  --source-url 'https://maps.jpl.nasa.gov/tmaps/jupiter.html' \
  --license 'publisher-confirmed redistribution terms' \
  --projection plate-carree \
  --positive west --left 0 --latitude planetographic \
  --expected-sha256 'YOUR_64_CHARACTER_INDEPENDENTLY_VERIFIED_SHA256'

globtoo --body io --body-imagery-dir io=/data/io-tiles
```

Replace the example's URL and license with the **exact** record for the
selected original image. An `--expected-sha256` value must be a real trusted
hash of those image bytes; the command stops before decoding if they differ.
If no independently obtained pin exists, **omit** `--expected-sha256`:
the script still records a locally calculated SHA-256 but explicitly labels
it **unverified against an external reference**. A local checksum is useful
for reproducibility; it is not proof of a publisher's source or license.

The output can immediately be used with `--body-imagery-dir body=DIR`.
It includes `manifest.txt`, power-of-two `tile-*.jpg`, `preview.jpg`,
`preview-small.jpg`, `source.txt`, an auditable `source.json` (input digest,
original coordinate assertions and hashes of every generated image/manifest),
plus `alignment.png` and `alignment.json` with the landmark checks.
Inspect the alignment PNG rather than trusting an automatically inferred
orientation; **nothing is automatically inferred**. This output is a
ready-to-use **local** map, not an addition to the source-controlled downloader.

The command refuses to overwrite an existing output directory unless
`--replace` is given. Conversion runs in a sibling staging directory first;
a failed conversion preserves the prior output. On replacement, the previous
directory is temporarily renamed out of the way and restored if publication
fails. A process killed during replacement may leave a `.backup-*` directory:
inspect it before deleting it. Source input is capped at 1 GiB and
250 megapixels; the tiler handles large maps by tile strips rather than
allocating a second full-size normalized raster. The output JSON records
operator-provided map metadata; it does not validate the source's scientific
datum, control network, or feature alignment automatically.

## Install a prepared planetary map (persistent user data)

A successful `prepare_planetary_map.py` run creates a complete, audited tile
package. Install it once, **without** rebuilding the viewer or supplying
`--body-imagery-dir` on every run:

```sh
python3 tools/install_planetary_map.py /data/io-tiles --verify
python3 tools/install_planetary_map.py /data/io-tiles
# Subsequent launches, including from a Nix installation:
globtoo --body io
```

The installer reads `source.json`, validates the declared target body and
coordinate convention, checks the manifest's tile grid and SHA-256 of **every
listed file**, and rejects unexpected files and symlinks. It copies the map
into `$XDG_DATA_HOME/globtoo/imagery/io/` (normally
`$HOME/.local/share/globtoo/imagery/io/`). Moon and Mars use equivalent
paths. An explicit `--body-imagery-dir` or `GLOBE_ASSET_DIR` overrides the
user installation. A user-installed map overrides an older map packaged by
CMake/Nix, and doesn't modify a read-only Nix store. To replace a previously
installed map, pass `--replace`; validation and the copy happen in a sibling
staging directory before atomic publication, restoring the previous directory
if publication fails. To uninstall a map, remove only its specific
`$XDG_DATA_HOME/globtoo/imagery/<body>` directory.

Only the **outputs from the current preparer** are accepted: `source.txt` is
now included among the SHA-256-checked files. Older Stage 10 outputs should
be re-prepared with the current version before installation. Source file
hashes and coordinate claims are recorded, not independently authenticated:
the operator must still check the publisher's map metadata and rights.
Installing a map is offline; it does not contact NASA/JPL or download assets.

## Include audited maps in a CMake/Nix installation

An existing, prepared map can also be included directly in a system build,
without re-tiling and without copying the original high-resolution mosaic:

```sh
cmake -S . -B build -DGLOBTOO_MARS_PREPARED_MAP=/data/mars-tiles
cmake --build build
cmake --install build --prefix "$HOME/.local"
# share/globtoo/imagery/mars/{manifest.txt,preview.jpg,tile-*.jpg,...}
```

The same option exists for `GLOBTOO_MOON_PREPARED_MAP` and
`GLOBTOO_IO_PREPARED_MAP`. Each must be a complete output of
`prepare_planetary_map.py`; configuration and the build validate its full
checksum manifest and reject an asset prepared for a different body.
`PREPARED_MAP` cannot be combined with the older `TEXTURE` or
`IMAGERY_TEXTURE` build option for the same body. The ordinary CMake install
works without Nix and keeps the provenance and visual QA records with the
asset.

Downstream **Nix flakes** can provide fixed, audited store paths without
requiring Globtoo to invent upstream hashes or commit large textures:

```nix
# In a downstream flake's outputs for the matching system:
packages.globtoo-with-maps = globtoo.lib.${system}.withPlanetaryMaps {
  moon = ./audited-moon-tiles;
  mars = ./audited-mars-tiles;
  # io = ./audited-io-tiles;
};
```

`./audited-*-tiles` must be a reproducible, already prepared directory in the
downstream flake or an independently checksum-pinned Nix derivation. The
corresponding `withPlanetaryMapsLite` function packages those tiles without
bundling the Earth image. The ordinary `packages.default` and
`packages.globtoo-lite` remain unchanged and do not acquire unverified maps.

## Preparing an externally obtained map

The input must be a color/grayscale **full 360° × 180° north-up plate carrée**
image, with exactly 2:1 dimensions (uniform longitude and latitude spacing),
no added map borders or legend. TIFF/GeoTIFF support depends on Pillow's
installed codecs; other supported raster formats include PNG and JPEG. The
script **does not reproject** Mercator, polar stereographic, regional mosaics,
or arbitrary geographic TIFF georeferencing. The optional Mars
planetographic-to-planetocentric conversion handles **latitude only**, on a
biaxial reference ellipsoid; other map projections still require planetary-aware
GIS preprocessing. Inspect the product's original projection and control-network
metadata first.

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

If your **Mars** source is in the older west-positive **planetographic**
system (common on pre-2002 imagery), both the longitude and latitude need
conversion:

```sh
python3 tools/normalize_planet_texture.py mars_source.tif mars.jpg \
  --positive west --left 0 --body mars --latitude planetographic
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

For example, given a locally obtained **planetographic, north-up, 2:1,
full-world** Io color mosaic that has west-positive longitudes and a 0°W
left boundary (verify these claims against the actual product metadata):

```sh
python3 tools/build_raster_tiles.py /data/io-source.tif /data/io-tiles \
  --positive west --left 0 --body io --latitude planetographic

globtoo --body io --body-imagery-dir io=/data/io-tiles
```

For a full-resolution **Mars planetographic** mosaic, add
`--body mars --latitude planetographic` to the tile converter. Each tile
resamples its north/south scanlines using Mars's reference ellipsoid
(equatorial radius 3396.2 km, polar radius 3376.2 km), without materializing
a second copy of the large image. The output preview uses the same conversion.
The test suite checks tile/single-texture agreement on a synthetic mosaic.
Longitude conversion also applies when the source is west-positive.

The Moon and Io Gazetteer spherical reference uses numerically equal
planetographic/planetocentric latitude; `--latitude planetographic` with
`--body moon|io` records that fact without changing their pixel rows.

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
  -DGLOBTOO_IO_LATITUDE_TYPE=planetographic \
  '-DGLOBTOO_IO_SOURCE=Exact product identifier and URL' \
  '-DGLOBTOO_IO_LICENSE=Verified redistribution terms'
cmake --build build
cmake --install build
```

Set `GLOBTOO_MARS_LATITUDE_TYPE=planetographic` for an older
planetographic Mars raster; CMake converts it to the planetocentric viewer
frame and records the source convention. Set `planetocentric` for sources
already in that frame (do not apply the warp twice).

Equivalent `GLOBTOO_MOON_IMAGERY_TEXTURE` and
`GLOBTOO_MARS_IMAGERY_TEXTURE` inputs are available, including when a small
per-body `*_TEXTURE` is not provided. The installed tiles include a
`source.txt` record of the supplied provenance and original longitude
convention. CMake rejects missing source metadata, unsupported longitude
orientation or absent input files. The default packages do **not** bundle
unverified source mosaics.

This is longitude-axis normalization with **optional Mars latitude**
reprojection, **not** a general cartographic transformation. Global maps with
shifted prime meridians, regional swaths, positive-south latitudes, 360°
endpoint duplication and non-equirectangular projections still require
planetary-aware GIS tools before tiling. The input's width must equal exactly twice its height.

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
- Planetographic Mars latitudes can be reprojected using the declared
  biaxial reference ellipsoid. Longitude normalization does **not** repair
  shifted scientific prime meridians, control networks, or arbitrary datums.
- Planetary high-resolution tiling and a small sourced landmark catalog exist.
  A complete gazetteer import and body-specific terrain relief remain future
  work. Existing measured elevation, population, cities, rivers and country
  boundaries are Earth-only.
- Hardware rendering and the geographic alignment of actual mosaics need
  testing with the original verified datasets, not just synthetic fixtures.

## Reproducible Nix installation (Stage 13)

The default `nix build` / `nix run` packages Earth, Moon, Mars and Io. The
planetary data is **not** committed to Git: `flake.nix` pins each upstream
image with a fixed-output hash, performs the oriented plate-carrée tile
conversion during the build, and installs `share/globtoo/imagery/<body>/`
(manifest, tiles, previews, source record). The `preview-small.jpg` is also
installed as `share/globtoo/assets/<body>.jpg` so both the distant globe and
close zoom show the same map.

Sources and limitations:

- Moon and Mars: Solar System Scope 8K maps (`8k_moon.jpg`, `8k_mars.jpg`),
  CC BY 4.0. The hash pins are published in
  [Orrery's ASSETS.md](https://github.com/RTF22/Orrery/blob/master/ASSETS.md);
  the author fills some missing data with illustrative terrain. Map placement
  (east-positive, −180° at the left) is a rendering assumption pending
  comparison against authoritative crater/landmark coordinates.
- Io: USGS Galileo/Voyager photomosaic mirrored by
  [Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Io_for_GeoHacks.jpg),
  public domain. The upstream SHA-1 hash is published in Commons file
  metadata. The source is 1225×613 pixels, one column short of an exact 2:1
  cylindrical texture, so Nix resamples it to 1226×613 before tiling. The
  cartographic input is treated as west-positive, 0° at the left, with
  spherical latitude. Visual registration against IAU features is outstanding.

The package choices are `default` (Earth and all three bodies),
`globtoo-earth` (full Earth assets only), and `globtoo-lite` (no bundled
textures; the explicit Earth downloader is retained). Changing the map
source or its hash in `flake.nix` should be accompanied by verifying the
source's licensing and cartographic coordinate convention. A Nix hash
ensures byte identity; it does not establish scientific georegistration.

Downstream flakes can override just one body with a previously audited map:

```nix
let globtoo = inputs.globtoo.lib.${system}.withPlanetaryMaps {
      mars = ./prepared-mars-tiles;
    };
in globtoo
```

The other bundled bodies remain unchanged. With `withPlanetaryMapsLite`, only
the explicitly passed audited maps are included. For already-installed users,
`tools/install_planetary_map.py` publishes maps to XDG user data and the viewer
uses their preview and tiles together ahead of the package's bundled assets.

**Build limitation:** These raster source URLs and externally published hashes
have not yet been fetched/recomputed in the development sandbox, which lacks
network access. Full Nix evaluation, package builds and GLES2 rendering must
be verified on NixOS before treating the image pins and axis assumptions as
fully validated.
