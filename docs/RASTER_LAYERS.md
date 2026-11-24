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
- Sources wider than 2048 pixels also get a **tile pyramid**: every coarser
  level halves the height (rounding up, keeping exact 2:1) until a level
  would no longer be sharper than the 2048-pixel preview. A 21600×10800
  source gives levels of 21600, 10800, 5400 and 2700 pixels; an 8192 Moon
  gives 8192 and 4096. Level 0 keeps the `tile-RRR-CCC.jpg` names, level
  *k* uses `tile-lK-RRR-CCC.jpg` in the same flat directory.
- `manifest.txt` records original dimensions, tile size, gutters, level-0
  columns and rows, and the level count (`GLOBTOO_RASTER_TILES 2`, shared by
  `tools/raster_manifest.py` and `src/tiled_raster.cpp`). Single-level
  `GLOBTOO_RASTER_TILES 1` manifests (no level count) remain valid; elevation
  and population tiles still use them. `preview.jpg` is a 2048×1024 world
  map with a 1024×512 `preview-small.jpg` fallback for smaller GLES2 devices.
  The source raster is not installed or committed to Git.
- Tile selection is by **on-screen texel density**, not a fixed zoom
  threshold. For every tile that can be visible (the globe's horizon cap,
  tightened to the viewport's corner rays up close; in flat maps a cap
  around the map centre), the renderer computes the pixels per radian of
  surface at the tile's nearest and farthest visible points and requests
  every level that some visible point needs: the coarsest level that is
  still at least as sharp as the screen. Nothing is requested where the
  base image (the regular 4K Earth map where available, otherwise the
  preview) is already sharp enough. If a selection would exceed the
  32-tile GPU cache (~170 MiB with mipmaps), the whole selection is
  coarsened one level at a time.
- JPEG decoding runs on a background thread per tile set; the render thread
  uploads at most two decoded tiles per frame. The LRU cache only evicts
  tiles the current frame does not need.
- Every loaded visible tile is drawn, finest level first, and stencil bit 1
  marks covered pixels, so each pixel shows the finest loaded tile and the
  base image fills the rest. While a tile streams in, coarser tiles or the
  base stay visible instead of the whole view waiting for the last tile.
  Projection morphs deform the surface (it may overlap itself) and draw the
  base image only. Stencil bit 0 still marks the globe silhouette for
  country, river and border overlays.
- Tile meshes are tessellated proportionally to their angular size
  (about 1.5° per segment), so coarse tiles keep a round limb.
- Each tile is drawn as a region of the **same sphere/projection geometry**, not
  an independent planar billboard. Border overlays, cities, rivers and
  markers continue to use the shared geographic frame.
- Detailed tiles require a GPU reporting `GL_MAX_TEXTURE_SIZE >= 1024`.
  There is no networking, progress bar or terrain displacement in this
  preview/tile pipeline; numerical elevation and population density have
  separate measured-data layers. Tiled visuals still need testing on
  older GLES2 devices.

**Credit:** NASA Earth Observatory / Reto Stöckli and NASA Blue Marble
Next Generation; underlying bathymetric shading uses GEBCO data. See
[NASA's source description](https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/topography-bathymetry-maps/).
