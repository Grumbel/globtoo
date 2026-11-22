<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Geographic points and rivers

Globtoo's **Capitals**, **Cities** and **Rivers** toggles are independent of
countries, borders, names and raster imagery. Natural Earth 10m point and
line features use the same geographic frame as the country layer, including
polar rotation, 2D projections, and horizontal wrap.

## Sources and reproducibility

The Nix flake fetches these **public-domain** files from the pinned Natural
Earth vector commit `ca96624a56bd078437bca8184e78163e5039ad19`
(version 5.1.2) with content SHA-256 verification:

| Source | SHA-256 (hex) |
|---|---|
| `ne_10m_populated_places_simple.geojson` | `fd3fa867a320cbd5c5b6bb5bc550afeec2939fb2cef688e508007282a55ac42f` |
| `ne_10m_rivers_lake_centerlines.geojson` | `bb854a900ecbd3b408df46d5e16e3e0f974ba55993f9d8b5c26e855273c0905a` |

See [Natural Earth populated places](https://www.naturalearthdata.com/downloads/10m-cultural-vectors/10m-populated-places/)
and [river centerlines](https://www.naturalearthdata.com/downloads/10m-physical-vectors/10m-rivers-lake-centerlines/).

No original GeoJSON is distributed in the installed package or Git bundle.
For an ordinary CMake build, copy the source GeoJSON to
`assets/places_10m.geojson` and `assets/rivers_10m.geojson` before configuring.
The same preprocessing occurs automatically; Python 3 is a **build-time-only**
dependency when both files are present.

## Build-time pipeline

`tools/build_geographic_features.py` reads `FeatureCollection` GeoJSON using
Python's standard library and emits `geographic_10m.gfeat` (little-endian
`GLBFEAT1`, version 1) to the generated asset directory. The file holds:

- Populated-place longitude and latitude, `NAME_EN`/`NAMEASCII`/`NAME`,
  `NAMEALT` (if available), `POP_MAX`, `SCALERANK`, and `ADM0CAP` flag.
- River segments with two unit-sphere endpoints, and source `SCALERANK`.
  Long arcs are sampled at most every 0.5° on the sphere before upload.

The renderer validates the header, counts, coordinates and record lengths.
No huge GeoJSON parses or GEOS triangulation are needed at startup.

Capitals use gold diamond symbols. Other cities use smaller pale circles.
Both are projected to screen coordinates through `scene_project` so hidden
hemisphere points are culled. Labels are prioritized by capital status,
source scale rank and population estimate, then screened for overlap with
country labels, each other, and the GUI. To avoid clutter at world scale,
only major cities are shown; closer views show more of the 10m dataset.

Rivers use `GL_LINES`, three rank-bucket VBOs, and the same
`projection.glsl` path used for country borders. Fewer rank buckets are
submitted at distant zoom. The globe's depth/stencil buffer prevents lines
on the back hemisphere or beyond the globe silhouette from being shown.

**Limitations:** Natural Earth deliberately generalizes small watercourses
and uses indicative, sometimes outdated metropolitan-population estimates.
The source marks national capitals, including alternates; locations and
political status reflect Natural Earth's choices. Map zoom levels do not
magically add detail beyond the underlying 10m vectors. Complex projection
seams require real-GPU visual testing, especially during morph animations.
