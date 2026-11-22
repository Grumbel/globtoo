<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Earth raster layers — Stage 3, first increment

Globtoo now distinguishes **Imagery** (the existing, small single-texture
Earth map) and an optional **Relief** raster (an independently selectable,
high-resolution equirectangular image). The Relief button in the bottom toolbar
switches between the two; `--layer imagery|relief` selects the starting layer.
Relief is unavailable until a tile set is installed or supplied explicitly.
`TEXTURE` still independently controls whether raster imagery is visible,
including when country colors are enabled at 60% opacity.

No images are downloaded at runtime. The standard Nix package still ships its
pinned 4K image, **not** an unverified large download. The 21,600×10,800 NASA
image is an optional build input in this increment. This is a *colorized
cartographic relief and bathymetry visualization*, not a measured elevation
raster; its colors must not be interpreted as meters of elevation or depth.
Numerical thematic rasters and their legends are a separate roadmap item.

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
  the preview uses a single draw call. The detailed tiles are chosen by
  geographic angular bounds for sufficiently close views and uploaded lazily
  from local disk, at most three tiles per frame. A 24-tile LRU cache bounds
  GPU memory; until every needed tile is ready, the preview remains visible.
- Each tile is drawn as a region of the **same sphere/projection geometry**, not
  an independent planar billboard. Border overlays, cities, rivers and
  markers continue to use the shared geographic frame.
- Detailed tiles require a GPU reporting `GL_MAX_TEXTURE_SIZE >= 1024`.
  There is no networking, progress bar, terrain displacement, numerical depth
  legend or population-density layer yet. Tiled visuals across projection
  seams and the globe limb still need an interactive GPU test, especially on
  older GLES2 devices. Disk loading is bounded per frame but synchronous.

**Credit:** NASA Earth Observatory / Reto Stöckli and NASA Blue Marble
Next Generation; underlying bathymetric shading uses GEBCO data. See
[NASA's source description](https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/topography-bathymetry-maps/).
