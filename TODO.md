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

### Stage 3 — Earth surface and thematic map layers (in progress)
- [x] Add independent Imagery / optional Relief raster selection in GUI and
      `--layer`, preserving independent political-color blending and shading.
      Support external tile directories with `--relief-dir`; absent datasets
      do not change default imagery. Document the source and limitations.
- [x] Add optional measured land elevation and bathymetry from NOAA ETOPO1
      signed 16-bit metres data. Convert grid-registered height posts to
      cell centers, with source/year metadata and a signed metres legend;
      keep visual NASA Relief distinctly unmeasured.
- [x] Add an optional measured population density layer with explicit source
      year, angular resolution, units (people/km2), legend, log color ramp,
      no-data color and provenance. Support global EPSG:4326 GeoTIFF or raw
      float32 values, with build-time tiles and no runtime dependency.
- [x] Implement optional build-time GLES2 JPEG tile generation and local
      close-up tile cache with power-of-two mipmapping and preview fallback.
      Validate dateline gutters and source/image resolution metadata.
- [x] Extend the tiled renderer to ordinary Earth Imagery as well as Relief,
      with independent CMake tile generation, `--imagery-dir` CLI override,
      automatic close-zoom detail, and 4K fallback during globe/map morphs.
      Test normal and optional-data CMake builds with synthetic 2:1 imagery.
- [ ] Offer a verified higher-resolution raster in the default Nix package,
      using **tiled/mipmapped** textures
      (GLES2 maximum texture sizes vary). Avoid loading a 21600×10800 image
      as one giant GPU texture; choose visible/LOD tiles at zoom.
      NASA Blue Marble Next Generation 21600×10800 topography/bathymetry:
      https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/base-topography-bathymetry/
- [x] Add numerical ocean relief/bathymetry together with land elevation,
      including separate land/sea color ramps and documented depth units;
      source NOAA ETOPO1:
      https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO1/tiled/
- [x] Document NASA/CIESIN GPWv4 density as an eligible measured data source;
      the converter requires explicit year and source attribution.
      https://data.nasa.gov/dataset/gridded-population-of-the-world-version-4-gpwv4-population-density
- [ ] Validate a full-size GPWv4 density raster's GeoTIFF interpretation,
      observed coastline alignment, and redistribution terms; do not add
      multi-gigabyte downloads to the default package without verification.

### Stage 4 — other planetary bodies
- [ ] Refactor Earth-only data into a Body catalog (Earth, Moon, Mars, Io,
      then more): sphere radius, coordinate direction, prime meridian, body
      shape, rotation convention, map source and feature overlays.
- [ ] Support body-specific base maps and optional shaded elevation; provide
      selector in UI and CLI, without showing Earth countries, cities, rivers,
      population, or Earth-based kilometer scale on other bodies.
- [x] Document USGS/IAU Gazetteer coordinate conventions for Moon, Mars and Io;
      convert older Mars planetographic full-globe plate carrée rasters onto
      the planetocentric sphere before importing tiles; keep longitude axes
      separate from latitude assumptions.
- [x] Add an offline visual map-registration QA sheet and tabular IAU feature
      pixel report for source and normalized tiled maps; test east/west
      canonical coordinates against the viewer's static feature table.
- [ ] Validate actual source raster coordinate/reference metadata (including
      control networks, positive-east/positive-west and prime meridian) and
      verify planetocentric alignment using external scientific datasets.

### Stage 5 — optional on-demand downloads and Nix split
- [x] Add an opt-in libcurl CLI downloader for the pinned Earth base map:
      XDG cache, HTTPS/TLS checks, SHA-256, size cap, progress, atomic writes,
      safe retry and meaningful failures. No network on normal launch.
- [x] Use the existing GLES2 tile renderer on Moon, Mars and Io with independent
      per-body caches, normalized east/west longitude at tile-preparation time,
      CLI overrides and opt-in source-attributed CMake packaging. Actual mosaic
      sources and live GLES2 alignment still require verification.
- [ ] Expand verified asset catalog to licensed Moon/Mars/Io data and
      high-resolution tiled sources; support resumable multi-part downloads
      and interactive UI consent/progress as appropriate.
- [x] Keep `packages.default` as the **fat**, offline Earth install;
      add `packages.globtoo-lite` omitting Earth imagery and 10m features,
      retaining 50m boundaries and opt-in libcurl Earth fetch. The default
      package has no libcurl/OpenSSL dependency from the downloader.
- [x] Register both Nix packages as build checks and add offline fixture tests
      for cache hit, corruption, digest failure, size limit, and cleanup.
- [ ] Run Nix checks on actual NixOS/SDL3/GLES2 hardware (not available in
      current headless development environment); verify `reuse lint` there.

## Next
- [ ] Test full-size GPWv4 population-density tiles and source metadata in
      an interactive GLES2 session and check the logarithmic scale visually.
- [ ] Test optional NOAA ETOPO1 grid with the complete 21601x10801 dataset,
      including signed colour range, poles, and source attribution.
- [ ] Pin and verify NASA high-resolution Imagery/Relief source hashes for Nix; test the
      complete fat package and visually check tile seams against the preview.
- [ ] Benchmark upload stutter, LRU size and visual transitions on real GLES2
      hardware; consider off-thread image decoding and per-tile frustum bounds.
- [ ] Verify Nix fetch hashes for 10m places/rivers and test full interactive
      GLES2 rendering on Linux (offline geometry and conversion tests pass).
- [ ] Optional proper Unicode/vector typeface with zoom-dependent country labeling
- [ ] More seamless antimeridian clipping while globe is mid-morph
- [ ] Live visual tests for GLES2, especially 10m datasets during map transitions
- [ ] Atmosphere rim / day-night / VR
- [ ] Optional finer-than-10m country-by-country boundaries (e.g. geoBoundaries)

### Stage 4a — planetary body selection
- [x] Select Earth, Moon, Mars, or Io by CLI (`--body`) and cycle using M or NEXT BODY.
- [x] Optional per-body equirectangular texture (`--body-texture body=FILE`).
- [x] Hide Earth country, city and river layers on non-Earth bodies; scale bar uses body radius.
- [x] Add test-covered normalization for externally supplied planet maps:
      west/east-positive longitude, 0/180 left boundary, seam-safe mirroring
      and half-turn, bounded single GLES2 texture size.
- [x] Support optional source-attributed CMake installation of Moon, Mars and
      Io imagery, with packaged fallback discovery and explicit CLI overrides.
- [x] Accept planetocentric, full-world external high-resolution planetary mosaics
      as normalized GLES2 tiles; use installed or CLI-supplied datasets.
- [x] Identify official NASA/JPL source pages and candidate low-resolution
      Moon, Mars and Io JPGs, documenting unverified bytes, axis conventions
      and redistribution/source-metadata limitations. No invented hashes.
- [x] Provide a one-command, offline source-map import workflow: explicit
      publisher/rights/coordinate metadata, optional supplied SHA-256 pin,
      per-body tiles and preview, audit JSON and IAU alignment QA, staging and
      failure-safe replacement. Actual NASA/JPL raster bytes remain unverified.
- [ ] Verify and package rights-cleared planetary raster maps, pole orientation
      and longitude direction per **actual** source, with pinned download hashes.
- [x] Source a small cross-body IAU Gazetteer feature-center catalog with
      original longitude/latitude conventions, non-Earth marker overlay and
      command-line --list-features / --focus navigation.
- [x] Document that the Gazetteer's Moon and Io planetographic latitudes
      use spherical cartographic coordinates and require no latitude warp.
- [ ] Import the full IAU planetary gazetteer, handle source map datums/control
      networks and validate feature alignment against real raster sources.
