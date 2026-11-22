<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Earth raster layers — Stage 3

Globtoo distinguishes **Imagery** (the ordinary Earth color map, with optional
high-resolution close-up tiles) and an optional **Relief** raster (an independently
selectable high-resolution equirectangular topography/bathymetry image). The
Relief button in the bottom toolbar switches between them;
`--layer imagery|relief` selects the starting layer.
Relief is unavailable until a tile set is installed or supplied explicitly.
`TEXTURE` still independently controls whether raster imagery is visible,
including when country colors are enabled at 60% opacity.

Measured elevation and bathymetry now have a **separate** ELEV layer using
signed 16-bit source rasters; see [ELEVATION.md](ELEVATION.md). The original
RELIEF layer remains a photographic visualization without physical units.

No images are downloaded at runtime. The standard Nix package still ships its
pinned 4K image, **not** an unverified large download. Optional 21,600×10,800
NASA imagery can now be used in the normal IMAGERY mode, as well as independently
for the RELIEF mode. A shaded-relief JPEG is a visualization, not a measured
elevation raster; colors must not be interpreted as meters of elevation or depth.
The separate ELEV and POP layers use numeric measurements and carry legends.

## High-resolution true-color Imagery (normal Earth mode)

The baseline `assets/earth.jpg` is kept for distant views and during projection
transitions; `imagery/` contains the optional close-up tiles. The tiles are
**not** an additional display mode: they are automatically selected when you
zoom into Imagery. Country colors, borders, and graticules still work over them.
The preview is also a fallback when the baseline image does not fit the GPU.

For example, NASA's [Blue Marble Next Generation base map](https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/base-map/)
offers north-up, global 21,600×10,800 JPEG imagery. Download and verify the
source file yourself, then run:

```sh
python3 tools/build_raster_tiles.py /path/to/world.200412.3x21600x10800.jpg \
  /tmp/globtoo-imagery
# Start any installed Globtoo and supply these tiles directly:
globtoo --imagery-dir /tmp/globtoo-imagery
```

Or have CMake preprocess and install them, while retaining a separate low-
resolution base texture:

```sh
cmake -S . -B build \
  -DGLOBTOO_EARTH_TEXTURE=/path/to/earth-4k.jpg \
  -DGLOBTOO_IMAGERY_TEXTURE=/path/to/earth-21600.jpg
cmake --build build -j
cmake --install build --prefix "$HOME/.local"
```

`share/globtoo/imagery/` contains only the generated manifest, previews, and
1024-pixel JPEG tiles; **the full-resolution source raster is not installed**.
This is compatible with the existing `--texture FILE` single-image override.
For truly close views, use the tiles instead of trying to upload one huge
21,600×10,800 RGBA texture through GLES2.

The `flake.nix` default remains reproducible with its already-pinned 4K
`fetchurl`. To include high-resolution imagery in a Nix derivation, supply a
**separately checksum-verified** source to the same
`-DGLOBTOO_IMAGERY_TEXTURE=/nix/store/...` CMake setting and add
`python3Packages.pillow` to `nativeBuildInputs`. This source has deliberately
not been enabled in the default flake without an independently verified
SHA-256 digest and a full Nix build. No network request occurs at application
startup.

## Preparing NASA Blue Marble relief

NASA's [Blue Marble Next Generation with Topography and Bathymetry](https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/base-topography-bathymetry/)
is equirectangular, with 21,600×10,800 and 5,400×2,700 JPEG variants.
The high-resolution original file is approximately 29 MB compressed, but
uploading it as RGBA would require ~890 MB of GPU memory, far beyond many
GLES2 texture-size limits. Prefer the 21,600-pixel version for close views:

```sh
curl --fail --location --output /tmp/world-relief.jpg \
  https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/world.topo.bathy.200412.3x21600x10800.jpg

# Build-time preprocessing (requires Python 3 + Pillow):
python3 tools/build_raster_tiles.py /tmp/world-relief.jpg /tmp/globtoo-relief

# Use the prepared tiles without recompiling the application:
globtoo --layer relief --relief-dir /tmp/globtoo-relief
```

Alternatively, install them at build time:

```sh
cmake -S . -B build -DGLOBTOO_RELIEF_TEXTURE=/tmp/world-relief.jpg \
  -DGLOBTOO_EARTH_TEXTURE=/path/to/current-earth.jpg
cmake --build build -j
cmake --install build --prefix "$HOME/.local"
```

The installed dataset resides in `share/globtoo/relief/`, separate from the
baseline `assets/earth.jpg`. CMake only requires Pillow when
`GLOBTOO_RELIEF_TEXTURE` is set. For Nix, a downstream derivation can pass an
already verified `fetchurl` output as this CMake option; the existing flake's
pinned Earth download and its SHA-256 are unchanged. We deliberately did not
add a new Nix fetch with an unverified hash.

## Technical layout and current limits

- `tools/build_raster_tiles.py` cuts a full-world JPEG into 1,024×1,024,
  power-of-two JPEG tiles. Each has four pixels of cyclic/clamped gutter for
  filtering at seams. The final column/row is edge-padded to 1,024 pixels.
- `manifest.txt` records original dimensions, tile size, gutters, columns and
  rows (`GLOBTOO_RASTER_TILES 1`). `preview.jpg` is a 2048×1024 world map with a 1024×512
  `preview-small.jpg` fallback for smaller GLES2 devices.
  The source raster is not installed or committed to Git.
- In distant globe views, flat-map overviews and while morphing projections,
  one base image (the regular 4K Earth map where available, otherwise the
  cache preview) uses a single draw call. The detailed tiles are chosen by
  geographic angular bounds for sufficiently close views and uploaded lazily
  from local disk, at most three tiles per frame. A 24-tile LRU cache bounds
  GPU memory; until every needed tile is ready, the corresponding base map
  (or the preview when no base texture is available) remains visible.
- Each tile is drawn as a region of the **same sphere/projection geometry**, not
  an independent planar billboard. Border overlays, cities, rivers and
  markers continue to use the shared geographic frame.
- Detailed tiles require a GPU reporting `GL_MAX_TEXTURE_SIZE >= 1024`.
  There is no networking, progress bar or terrain displacement in this
  preview/tile pipeline; numerical elevation and population density have
  separate measured-data layers. Tiled visuals across projection
  seams and the globe limb still need an interactive GPU test, especially on
  older GLES2 devices. Disk loading is bounded per frame but synchronous.

**Credit:** NASA Earth Observatory / Reto Stöckli and NASA Blue Marble
Next Generation; underlying bathymetric shading uses GEBCO data. See
[NASA's source description](https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/topography-bathymetry-maps/).
