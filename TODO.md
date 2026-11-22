<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# TODO.md — globtoo

## Current tip
- On-screen GLES2 controls for globe/map projections, shading, countries, borders, names, zoom
- Animated globe/2D morph and interpolated Equirectangular, Mercator, Mollweide, Hammer projections
- Start globe oriented to geographical TZ-reference city (or UTC meridian fallback)
- Build-time English `NAME_EN` label anchors; `.gmesh` v2 retains unwrapped longitude/latitude
- Correct right-handed east/west coordinates across geographic sphere, political mesh and stars
- Country overlay tests the globe depth without writing it and uses globe silhouette stencil
- Adaptive globe depth range when zoomed; stars use independent sky projection
- Third-party stb_image moved into external/
- Add 10m Natural Earth close-up country mesh with build-time preprocessing
- Keep 50m at distance and 10m at close zoom, both from identical border/fill meshes

## Done
- [x] Tab hides/restores screen-space controls without hiding geographic overlays; move Grab / Arcball below compass Reset (G shortcut)
- [x] CMake install target and Nix packaging through standard cmake install
- [x] Desktop launcher and AppStream metadata; custom multi-size icon and SDL window icon
- [x] Versioned man page, license and asset credits, release checklist
- [x] Installed shader/texture/country resource lookup independent of cwd
- [x] Middle-button screen-space grab/pan for 2D projections, separate from left-drag Earth rotation; seamless cylindrical wrap
- [x] Repeatable CLI markers (decimal, compass, DMS, decimal minutes, Geo URI,
      ISO 6709) with optional labels, CSV import, pin colors/sizes and explicit view center
- [x] Markers follow globe/map projection, occlusion and transformations
- [x] Single-source VERSION with CMake/Nix development metadata and `--version`
- [x] Optional adaptive geographic graticule in globe / all flat projections
- [x] Graduated, kilometer/meter great-circle scale bar at viewport center
- [x] Independently toggle country colors and Earth imagery; blend both when enabled
- [x] Interactive geographic compass, North-up alignment, and relocated full Reset (including roll)
- [x] Geographic reprojection from a full 3D rotating frame for north/south pole crossings (no mirrored map copies)
- [x] Shift+drag rotation for flat maps and preservation through globe folding
- [x] Seam-safe triangle/segment reprojection plus horizontal wrapping and zoom-aware mesh LOD
- [x] Extend close-up globe/map zoom limits; increase sphere tessellation and test clipping
- [x] Match GLES2 vertex/fragment precision on map morph uniforms and all varyings; regression-test linked interfaces
- [x] Document coordinate system (docs/COORDINATES.md)
- [x] Align sphere texture UVs with lon/lat, including the actual mesh seam
- [x] Political fill from MAPCOLOR7 with proper geometry, not centroid fans
- [x] Preserve depth for globe and country border/fill overlay
- [x] Geometry tests for holes, concavities, winding and antimeridian
- [x] Fix east/west mirroring and verify world/screen orientation
- [x] Prevent political overlay depth/stencil artifacts at globe silhouette
- [x] Move third-party headers to external/
- [x] Compile Natural Earth 50m and 10m GeoJSON to binary meshes at build time
- [x] Automatically select detailed borders at close zoom with LOD hysteresis
- [x] Test binary mesh serialization and geometry-only CMake build

## Development roadmap (ordered)

The checkboxes below distinguish shipped functionality from planned work.
Finish and validate one stage before making the next one the default; never
silently download large maps or satellite data on an existing installation.

### Stage 1 — interaction and low-risk rendering performance (completed)
- [x] Halve mouse-wheel zoom increments in globe and flat modes; make
      zoom-in/zoom-out reciprocal at ordinary distances.
- [x] Preserve 2D middle-drag sheet panning; add Shift+middle-drag roll in 2D
      and middle-drag roll on the 3D globe (same as Shift+left-drag).
- [x] Add persistent North Lock toggle to the compass panel and keep North-up
      when rotating the geographic frame; retain one-shot North Up.
- [x] Move the toolbar to the bottom and ruler to the right, beneath compass.
- [x] Cache GLES2 shader uniform locations once per program link.
- [x] Draw all world copies' political fills together, then their borders,
      reducing redundant program/VBO/vertex-format state changes.
- [x] Avoid allocating the toolbar button vector on every input/draw event,
      reuse UI vertex storage, and replace bitmap-glyph linear searches with
      a lookup table.
- [ ] Measure actual GL calls, frame time, upload bandwidth and CPU time
      (close-up 10m mesh, grid, labels, projection morph and 3× world wrap)
      on GLES2 hardware; avoid claiming reduced draw-call counts without data.
- [ ] Profile and batch/cull expensive meshes and line labels where beneficial;
      use counters and before/after benchmarks, avoid visual regressions.

### Stage 2 — geographic point and line features
- [x] Fetch Natural Earth **10m populated places** at a pinned version/hash;
      import capitals and major cities with name, prominence, population,
      alternate names and overlap-aware labels. Different symbols and separate
      toggles for capitals versus other major cities. Default to major cities
      only beyond an appropriate zoom threshold.
      Source: https://www.naturalearthdata.com/downloads/10m-cultural-vectors/10m-populated-places/
- [x] Add ranked major rivers and lake centerlines, with LOD and a toggle;
      project through exactly the same rotated frame as the country borders,
      avoiding dateline/polar seams, and cull invisible segments.
      Source: https://www.naturalearthdata.com/downloads/10m-physical-vectors/10m-rivers-lake-centerlines/

### Stage 3 — Earth surface and thematic map layers (next)
- [ ] Introduce a selectable texture/layer catalog (imagery, topographic
      elevation, population density, ocean bathymetry); preserve independent
      blending with political country colors and shading.
- [ ] Offer higher-resolution Earth imagery using **tiled/mipmapped** textures
      (GLES2 maximum texture sizes vary). Avoid loading a 21600×10800 image
      as one giant GPU texture; choose visible/LOD tiles at zoom.
      NASA Blue Marble Next Generation 21600×10800 topography/bathymetry:
      https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/base-topography-bathymetry/
- [ ] Add explicit ocean relief/bathymetry as a texture or separate overlay,
      with land/ocean color mapping, a legend and documented depth units;
      evaluate NOAA ETOPO1 tiles:
      https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO1/tiled/
- [ ] Add population density with year/resolution/legend metadata; consider
      NASA/CIESIN GPWv4. Verify each layer's download terms and attribution.
      https://data.nasa.gov/dataset/gridded-population-of-the-world-version-4-gpwv4-population-density

### Stage 4 — other planetary bodies
- [ ] Refactor Earth-only data into a Body catalog (Earth, Moon, Mars, Io,
      then more): sphere radius, coordinate direction, prime meridian, body
      shape, rotation convention, map source and feature overlays.
- [ ] Support body-specific base maps and optional shaded elevation; provide
      selector in UI and CLI, without showing Earth countries, cities, rivers,
      population, or Earth-based kilometer scale on other bodies.
- [ ] Verify planetocentric versus planetographic coordinates and IAU longitude
      direction (do not assume all bodies use Earth's east-positive frame);
      identify reliable NASA/USGS sources and per-asset licenses/checksums.

### Stage 5 — optional on-demand downloads and Nix split
- [ ] Build opt-in libcurl texture/tiles fetcher with explicit user consent,
      pinned manifest/checksums, XDG cache, size caps, offline behavior,
      interruption recovery, clear attribution and meaningful errors.
- [ ] Keep `packages.default` as the **fat**, fully functional offline install;
      add `packages.globtoo-lite` using the same executable but smaller
      packaged data and optional runtime downloads. Keep strict dependency
      separation: no libcurl requirement for the fat/offline build unless
      download functionality was explicitly enabled at compile time.
- [ ] Add end-to-end Nix checks for each package, launch without networking,
      cache hit/miss/corrupt cases, and `reuse lint` for new metadata.

## Next
- [ ] Verify Nix fetch hashes for 10m places/rivers and test full interactive
      GLES2 rendering on Linux (offline geometry and conversion tests pass).
- [ ] Optional proper Unicode/vector typeface with zoom-dependent country labeling
- [ ] More seamless antimeridian clipping while globe is mid-morph
- [ ] Live visual tests for GLES2, especially 10m datasets during map transitions
- [ ] Atmosphere rim / day-night / VR
- [ ] Optional finer-than-10m country-by-country boundaries (e.g. geoBoundaries)
