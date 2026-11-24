<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Planetary surface maps

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

The bundled **small** feature catalog contains 64 IAU-approved names and
feature-center coordinates from the [USGS Gazetteer of Planetary Nomenclature](https://planetarynames.wr.usgs.gov/). These are reference point labels,
not outlines and not a complete scientific GIS dataset. Every solid body
except Earth has two or three; the gas giants have none. Click **LANDMARKS**
(on any body with landmarks) or press **F** to toggle them. Use
`globtoo --list-features` to see the names, or start at one with
`globtoo --focus 'Olympus Mons'`. This switches to the correct body and
centers the camera. The labels work without any external texture.

| Body | Label | Source center coordinates | Gazetteer entry |
| --- | --- | --- | --- |
| Mercury | Caloris Planitia | 30.50°N, 189.80°W | [979](https://planetarynames.wr.usgs.gov/Feature/979) |
| Mercury | Rachmaninoff | 27.60°N, 302.40°W | [14653](https://planetarynames.wr.usgs.gov/Feature/14653) |
| Mercury | Kuiper | 11.30°S, 31.10°W | [3140](https://planetarynames.wr.usgs.gov/Feature/3140) |
| Venus | Maxwell Montes | 65.20°N, 3.30°E | [3766](https://planetarynames.wr.usgs.gov/Feature/3766) |
| Venus | Aphrodite Terra | 5.80°S, 104.80°E | [317](https://planetarynames.wr.usgs.gov/Feature/317) |
| Venus | Mead | 12.50°N, 57.20°E | [3787](https://planetarynames.wr.usgs.gov/Feature/3787) |
| Moon | Tycho | 43.30°S, 11.22°W | [6163](https://planetarynames.wr.usgs.gov/Feature/6163) |
| Moon | Copernicus | 9.62°N, 20.08°W | [1296](https://planetarynames.wr.usgs.gov/Feature/1296) |
| Moon | Mare Tranquillitatis | 8.35°N, 30.83°E | [3691](https://planetarynames.wr.usgs.gov/Feature/3691) |
| Mars | Olympus Mons | 18.40°N, 226.00°E | [4453](https://planetarynames.wr.usgs.gov/Feature/4453) |
| Mars | Gale | 5.44°S, 137.70°E | [2071](https://planetarynames.wr.usgs.gov/Feature/2071) |
| Mars | Hellas Planitia | 42.66°S, 70.00°E | [2432](https://planetarynames.wr.usgs.gov/Feature/2432) |
| Io | Loki Patera | 12.97°N, 308.80°W | [3459](https://planetarynames.wr.usgs.gov/Feature/3459) |
| Io | Pele | 18.71°S, 255.28°W | [4638](https://planetarynames.wr.usgs.gov/Feature/4638) |
| Io | Tvashtar Paterae | 62.76°N, 123.53°W | [14131](https://planetarynames.wr.usgs.gov/Feature/14131) |
| Europa | Pwyll | 25.20°S, 271.40°W | [4878](https://planetarynames.wr.usgs.gov/Feature/4878) |
| Europa | Conamara Chaos | 9.70°N, 272.70°W | [1282](https://planetarynames.wr.usgs.gov/Feature/1282) |
| Europa | Tyre | 33.60°N, 146.60°W | [6170](https://planetarynames.wr.usgs.gov/Feature/6170) |
| Ganymede | Galileo Regio | 45.00°N, 127.00°W | [2076](https://planetarynames.wr.usgs.gov/Feature/2076) |
| Ganymede | Osiris | 38.00°S, 166.31°W | [4509](https://planetarynames.wr.usgs.gov/Feature/4509) |
| Ganymede | Tros | 11.14°N, 27.26°W | [6091](https://planetarynames.wr.usgs.gov/Feature/6091) |
| Callisto | Valhalla | 14.70°N, 56.00°W | [6284](https://planetarynames.wr.usgs.gov/Feature/6284) |
| Callisto | Asgard | 32.20°N, 139.90°W | [420](https://planetarynames.wr.usgs.gov/Feature/420) |
| Callisto | Bran | 24.20°S, 205.60°W | [862](https://planetarynames.wr.usgs.gov/Feature/862) |
| Mimas | Herschel | 1.38°S, 111.76°W | [2478](https://planetarynames.wr.usgs.gov/Feature/2478) |
| Mimas | Arthur | 35.40°S, 196.04°W | [403](https://planetarynames.wr.usgs.gov/Feature/403) |
| Enceladus | Baghdad Sulcus | 86.91°S, 230.54°W | [14248](https://planetarynames.wr.usgs.gov/Feature/14248) |
| Enceladus | Ali Baba | 56.84°N, 17.51°W | [184](https://planetarynames.wr.usgs.gov/Feature/184) |
| Enceladus | Samarkand Sulci | 30.00°N, 327.50°W | [5291](https://planetarynames.wr.usgs.gov/Feature/5291) |
| Tethys | Odysseus | 32.82°N, 128.89°W | [4406](https://planetarynames.wr.usgs.gov/Feature/4406) |
| Tethys | Ithaca Chasma | 14.00°S, 6.10°W | [2751](https://planetarynames.wr.usgs.gov/Feature/2751) |
| Tethys | Penelope | 10.83°S, 249.22°W | [4645](https://planetarynames.wr.usgs.gov/Feature/4645) |
| Dione | Evander | 57.00°S, 145.00°W | [14353](https://planetarynames.wr.usgs.gov/Feature/14353) |
| Dione | Padua Chasmata | 17.70°N, 247.17°W | [14384](https://planetarynames.wr.usgs.gov/Feature/14384) |
| Dione | Aeneas | 25.89°N, 46.27°W | [64](https://planetarynames.wr.usgs.gov/Feature/64) |
| Rhea | Tirawa | 34.20°N, 151.70°W | [6026](https://planetarynames.wr.usgs.gov/Feature/6026) |
| Rhea | Inktomi | 14.10°S, 112.10°W | [14671](https://planetarynames.wr.usgs.gov/Feature/14671) |
| Rhea | Mamaldi | 14.00°N, 184.00°W | [14751](https://planetarynames.wr.usgs.gov/Feature/14751) |
| Titan | Xanadu | 15.00°S, 100.00°W | [6958](https://planetarynames.wr.usgs.gov/Feature/6958) |
| Titan | Kraken Mare | 68.00°N, 310.00°W | [14399](https://planetarynames.wr.usgs.gov/Feature/14399) |
| Titan | Menrva | 20.10°N, 87.20°W | [7025](https://planetarynames.wr.usgs.gov/Feature/7025) |
| Iapetus | Cassini Regio | 28.10°S, 92.60°W | [1047](https://planetarynames.wr.usgs.gov/Feature/1047) |
| Iapetus | Engelier | 40.50°S, 264.70°W | [14463](https://planetarynames.wr.usgs.gov/Feature/14463) |
| Iapetus | Turgis | 16.90°N, 28.40°W | [14488](https://planetarynames.wr.usgs.gov/Feature/14488) |
| Miranda | Inverness Corona | 66.90°S, 325.70°E | [2704](https://planetarynames.wr.usgs.gov/Feature/2704) |
| Miranda | Verona Rupes | 18.30°S, 347.80°E | [6359](https://planetarynames.wr.usgs.gov/Feature/6359) |
| Miranda | Arden Corona | 29.10°S, 73.70°E | [353](https://planetarynames.wr.usgs.gov/Feature/353) |
| Ariel | Yangoor | 68.70°S, 279.70°E | [6638](https://planetarynames.wr.usgs.gov/Feature/6638) |
| Ariel | Kachina Chasmata | 33.70°S, 246.00°E | [2879](https://planetarynames.wr.usgs.gov/Feature/2879) |
| Umbriel | Wunda | 7.90°S, 273.60°E | [6587](https://planetarynames.wr.usgs.gov/Feature/6587) |
| Umbriel | Vuver | 4.70°S, 311.60°E | [6460](https://planetarynames.wr.usgs.gov/Feature/6460) |
| Titania | Messina Chasmata | 33.30°S, 335.00°E | [3870](https://planetarynames.wr.usgs.gov/Feature/3870) |
| Titania | Gertrude | 15.80°S, 287.10°E | [2150](https://planetarynames.wr.usgs.gov/Feature/2150) |
| Oberon | Hamlet | 46.10°S, 44.40°E | [2340](https://planetarynames.wr.usgs.gov/Feature/2340) |
| Oberon | Mommur Chasma | 16.30°S, 323.50°E | [3958](https://planetarynames.wr.usgs.gov/Feature/3958) |
| Triton | Leviathan Patera | 17.00°N, 28.50°E | [3368](https://planetarynames.wr.usgs.gov/Feature/3368) |
| Triton | Tuonela Planitia | 34.00°N, 14.50°E | [6134](https://planetarynames.wr.usgs.gov/Feature/6134) |
| Triton | Mazomba | 18.50°S, 63.50°E | [3771](https://planetarynames.wr.usgs.gov/Feature/3771) |
| Pluto | Sputnik Planitia | 19.51°N, 178.69°E | [15669](https://planetarynames.wr.usgs.gov/Feature/15669) |
| Pluto | Tombaugh Regio | 7.62°N, 183.22°E | [15668](https://planetarynames.wr.usgs.gov/Feature/15668) |
| Pluto | Burney | 45.68°N, 133.79°E | [15680](https://planetarynames.wr.usgs.gov/Feature/15680) |
| Charon | Dorothy | 58.53°N, 40.58°E | [15738](https://planetarynames.wr.usgs.gov/Feature/15738) |
| Charon | Argo Chasma | 27.80°N, 80.40°E | [15734](https://planetarynames.wr.usgs.gov/Feature/15734) |
| Charon | Kubrick Mons | 3.60°N, 30.80°E | [15736](https://planetarynames.wr.usgs.gov/Feature/15736) |

**Coordinates are not interchangeable between bodies.** Each row keeps the
system the Gazetteer publishes for that body: lunar entries are
planetographic latitude, east-positive longitude in −180–180; Martian entries
are planetocentric, east-positive 0–360; Mercury, Io, Europa, Ganymede,
Callisto and the Saturnian moons are planetographic and **west-positive**
0–360; Venus, the Uranian moons, Pluto and Charon are planetocentric,
east-positive 0–360; Triton is planetographic, east-positive 0–360. Globtoo
flips west-positive longitudes and wraps all of them to [−180°, 180°)
east-positive. According to
[USGS Gazetteer coordinate guidance](https://planetarynames.wr.usgs.gov/Page/Website),
every body listed here with planetographic entries has a **spherical
cartographic reference**, so planetographic and planetocentric **latitude are
numerically identical** for those entries; their labels need no latitude
correction (the tests enforce that Mars, the one oblate body with landmarks,
is listed planetocentric). Differences from an actual mosaic's control
network, underlying shape and projection are still
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
`$HOME/.local/share/globtoo/imagery/io/`). Every other body uses the
equivalent path under its lower-case name. An explicit `--body-imagery-dir` or `GLOBE_ASSET_DIR` overrides the
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

The same option exists for every body except Earth, e.g.
`GLOBTOO_MOON_PREPARED_MAP` or `GLOBTOO_TITAN_PREPARED_MAP`. Each must be a complete output of
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

The input must be a color/grayscale **full 360° × 180° plate carrée**
image (north-up, or south-up with `--top south`), with exactly 2:1 dimensions (uniform longitude and latitude spacing),
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

For ±180° maps, use `--left 180`, regardless of positive direction. A map
whose top row is the **south** pole takes `--top south` (all of the
normalizer, tiler, alignment and preparation tools accept it). A raster
rotated by 180° is south-up **and** reversed in longitude: declare it with
`--top south --positive west`, exactly as stored, never by guessing. The
preprocessor mirrors west-positive pixels and rotates 0–360° images by half
a revolution; its operations have exact pixel-center alignment, including
the antimeridian seam and both poles. Downsampling occurs before rearranging
the pixels. The maximum width defaults to 2048 to avoid exceeding typical
GLES2 texture limits; the runtime further checks `GL_MAX_TEXTURE_SIZE`.

Run with `globtoo --body moon --body-texture moon=/path/to/moon.jpg`, or install
one of the normalized maps as `share/globtoo/assets/<body>.jpg`.
Globtoo discovers installed planetary maps when switching bodies, while an
explicit `--body-texture` overrides the installed file. The application does
not download any textures without user action.

## High-resolution tiled planetary imagery

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
`share/globtoo/imagery/<body>/` for automatic detection:

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
same options exist for every other body (`GLOBTOO_MARS_*`, `GLOBTOO_IO_*`,
`GLOBTOO_TITAN_*`, ...), including `GLOBTOO_<BODY>_TOP=north|south`. The normalized
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

The default `nix build` / `nix run` packages Earth and **every other body in
`data/bodies.tsv`**: all planets, the major moons, Pluto and Charon. The
planetary data is **not** committed to Git: `flake.nix` pins each upstream
image with a fixed-output hash and runs `tools/prepare_planetary_map.py` on it
in a separate per-body derivation (`.#planetary-map-<body>`, or all of them as
`.#planetary-maps`). Those derivations only depend on the source image, the
tiling tools and the body/landmark catalogs, so changing application code does
not re-tile any imagery. Each result is a regular audited prepared map
(manifest, tiles, previews, `source.txt`/`source.json`, alignment QA sheet),
handed to CMake as `GLOBTOO_<BODY>_PREPARED_MAP` and installed as
`share/globtoo/imagery/<body>/`. The `preview-small.jpg` is also
installed as `share/globtoo/assets/<body>.jpg` so both the distant globe and
close zoom show the same map. `postInstall` audits every installed tile grid.

Each source's orientation is **declared** in `flake.nix` and was checked with
`tools/planetary_alignment.py` QA sheets against the landmark catalog above
(gas giants have no surface landmarks; their maps are cloud-top textures
without a fixed surface frame):

| Body | Source | Longitude, left edge, top | Landmark QA |
| --- | --- | --- | --- |
| Moon, Mars | Solar System Scope 8K, CC BY 4.0 | east, 180°, north | Tycho, Copernicus, Olympus Mons, Hellas match |
| Mercury | Solar System Scope 8K | east, 180°, north | Kuiper, Rachmaninoff, Caloris match |
| Venus | Solar System Scope 8K (`8k_venus_surface`) | **west, 180°, south** (rotated 180°) | Maxwell Montes, Aphrodite Terra match |
| Jupiter, Saturn, Uranus, Neptune | Solar System Scope | east, 180°, north | n/a (cloud tops) |
| Io | USGS Galileo/Voyager, Commons | **east, 180°**, north | Pele ring, Loki, Tvashtar match (the earlier west/0° declaration was wrong) |
| Europa, Ganymede, Callisto | NASA/JPL Voyager maps, Commons | east, 180°, north | Pwyll, Tyre, Valhalla, Asgard match |
| Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus | Cassini color maps (PIA184xx), Commons | east, 0°, north | Herschel, Odysseus, Engelier, Cassini Regio match |
| Titan | Cassini ISS map, April 2011, Commons | east, 0°, north | Menrva ring and Xanadu match |
| Miranda, Ariel, Umbriel, Titania, Oberon | NASA/JPL/USGS Voyager maps, Commons | east, 180°, north | Arden Corona, Kachina Chasmata, Wunda, Messina Chasmata, Hamlet match |
| Triton | Voyager map (P. Schenk, LPI), Commons | east, 180°, north | Leviathan Patera on imaged terrain |
| Pluto | New Horizons color mosaic, Commons | east, 0°, north | Sputnik Planitia / Tombaugh Regio match |
| Charon | New Horizons map (IAU iau1803c), Commons | east, 180°, north | Kubrick Mons, Argo Chasma match |

Solar System Scope files are pinned with SRI SHA-256 digests; Commons files
are pinned with the SHA-1 published by the Commons imageinfo API, like Io.
Io (1225×613), Tethys (13467×6734), Rhea (12015×6008) and Iapetus
(11741×5871) are a pixel or two off an exact 2:1 grid; the build resamples
their width with `tools/fit_plate_carree.py`, which refuses a source whose
pinned size changed or whose aspect deviates by more than 1%. Voyager-era
maps of the Uranian moons, Triton, and the New Horizons maps of Pluto and
Charon have large unimaged regions (black or filled), which is how the
original products are published.

The package choices are `default` (Earth and all other bodies),
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
