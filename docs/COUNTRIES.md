# Country borders and political coloring

Natural Earth Admin-0 country polygons (public domain), fetched at build time
by the Nix flake in **1:50m** and **1:10m** resolutions. The 10m source is
pinned to Natural Earth v5.1.2 in `flake.nix`; both source GeoJSON files are
converted by `country_mesh_compiler` during the CMake build. The Nix package
ships only versioned `.gmesh` outputs, not the large GeoJSON files. The 50m
mesh renders when the globe is distant, switching to 10m for close-up views.

`src/country_mesh_io.cpp` reads/writes the exact pretriangulated float vertices
and border strips (little-endian binary format `GLOBGM01`), with length and
strip-range validation. No expensive geometry processing occurs at application
startup when the compiled assets are available. The LOD transition has
hysteresis: enters 10m under camera distance 1.85, returns to 50m above 2.15
(radius of Earth is 1). Drawing both fills and outlines from the *same* LOD
prevents seams from mismatched detail levels. If the 10m asset is unavailable,
the 50m asset is used throughout; a legacy `assets/countries.geojson` still
works for local development.

## Geometry

The independent CPU geometry builder (`src/country_geometry.cpp`) parses
`FeatureCollection`/`Polygon`/`MultiPolygon` geometry and `MAPCOLOR7`. For each
polygon, its outer ring and all interior holes are passed to **GEOS constrained
Delaunay triangulation**. This replaces the incorrect centroid triangle fan,
which covered concavities, lakes, the opposite hemisphere, and sometimes entire
oceans. Requires GEOS >= 3.10 (`geos_c`); Nix supplies it automatically.

Country boundaries (exterior rings and holes) are sampled to at most 0.5°
*before* GEOS constrained triangulation. The **same sampled ring vertices**
are used for the line strip and as constrained triangle edges. A border is
therefore coincident with the fill edge at any zoom; interpolating two meshes
independently caused gaps and overlaps along the coast and political borders.
For self-intersecting polygons, both borders and fills use the boundary after
`GEOSMakeValid`, rather than outlining the original invalid polygon.

Triangles are subdivided in *unwrapped longitude/latitude* to at most 2.5°
per edge, then projected onto the Y-up sphere so their interior follows the
Earth instead of disappearing below the ocean. All triangles wind CCW when
viewed from outside. **Fills and borders both use radius 1.0015**. Both layers test depth against
only the underlying globe and do not write depth. Border lines are drawn last
with no polygon offset and no separate radial lift, so neither layer can
occlude the other. Polygon holes stay ocean. Smaller sampled boundary segments
make outlines smooth at close zoom.

Dateline-crossing polygons are unwrapped locally; +180°/-180° polar cut edges
are retained so the Antarctic cap doesn't become a 360° closing chord. This is
independent of the globe's *texture* seam at +/-180°.

## Visibility

The globe is rendered first and its **depth buffer is preserved**. Countries
and borders use depth testing, so their far-side geometry is hidden by the
solid globe. Face culling alone is insufficient, as `GL_LINE_STRIP` is not
culled. Do not clear depth between drawing the globe and the countries.

The political geometry sits at radius 1.0015 so it remains in front of the
base globe, but that radial lift would cause thin strips to peek outside the
silhouette at the limb. The globe also writes a **stencil mask (value 1)**
when it passes depth; political fills/lines require stencil == 1. The SDL
context requests 8 stencil bits and the frame clears stencil together with
depth. A short distance-dependent near/far clip range dramatically improves
depth precision when zoomed in; stars use a separate sky projection.

All country positions use the shared right-handed `geo::lonlat_to_xyz` mapping
(`+X` at Greenwich, `−Z` at 90°E). The globe sphere texture and geographic
outlines now agree without east/west mirroring.

The seven-color palette uses Natural Earth's `MAPCOLOR7` (1..7), with a stable
name-based fallback if missing. The camera light vector points *toward* the
eye, so colors aren't confined to the shader's ambient term.

## Verification

Run `ctest --test-dir build --output-on-failure`. The CPU build and tests can
also run without SDL3/GLES by configuring with `-DGLOBTOO_BUILD_APP=OFF`.
Unit coverage includes
concave polygons, holes, antimeridian crossings, winding, sphere radius,
per-country colors, self-intersection repair, exact line/triangle boundary
correspondence (including holes), and malformed input, binary mesh round-trips/truncation, and LOD switching.
A real Natural Earth
GeoJSON dataset can also be supplied to a geometry-only integration test
without an SDL window.
