# Country borders and political coloring

Natural Earth Admin-0 countries (public-domain GeoJSON), fetched at build time
by the Nix flake. The default dataset is **1:50m**, including offshore islands
and interior rings. See `flake.nix` for the pinned asset URL and hash.

## Geometry

The independent CPU geometry builder (`src/country_geometry.cpp`) parses
`FeatureCollection`/`Polygon`/`MultiPolygon` geometry and `MAPCOLOR7`. For each
polygon, its outer ring and all interior holes are passed to **GEOS constrained
Delaunay triangulation**. This replaces the incorrect centroid triangle fan,
which covered concavities, lakes, the opposite hemisphere, and sometimes entire
oceans. Requires GEOS >= 3.10 (`geos_c`); Nix supplies it automatically.

Triangles are subdivided in *unwrapped longitude/latitude* to at most 2.5° per
edge, then projected onto the Y-up unit sphere. This makes even large inland
triangles follow the sphere instead of intersecting it as planar chords.
All triangle windings are made CCW when viewed from outside the globe, and
triangles are lifted 0.15% above the ocean sphere. Borders use the same
coordinates, with 1° subdivision and a 0.25% lift. Polygon holes remain ocean.

Dateline-crossing polygons are unwrapped locally; +180°/-180° polar cut edges
are retained so the Antarctic cap doesn't become a 360° closing chord. This is
independent of the globe's *texture* seam at +/-180°.

## Visibility

The globe is rendered first and its **depth buffer is preserved**. Countries
and borders use depth testing, so their far-side geometry is hidden by the
solid globe. Face culling alone is insufficient, as `GL_LINE_STRIP` is not
culled. Do not clear depth between drawing the globe and the countries.

The seven-color palette uses Natural Earth's `MAPCOLOR7` (1..7), with a stable
name-based fallback if missing. The camera light vector points *toward* the
eye, so colors aren't confined to the shader's ambient term.

## Verification

Run `ctest --test-dir build --output-on-failure`. Unit coverage includes
concave polygons, holes, antimeridian crossings, winding, sphere radius,
per-country colors, and malformed input. A real Natural Earth dataset can
also be supplied to a geometry-only integration test without an SDL window.
