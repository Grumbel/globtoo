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

## Next
- [ ] Optional proper Unicode/vector typeface with zoom-dependent country labeling
- [ ] More seamless antimeridian clipping while globe is mid-morph
- [ ] Live visual tests for GLES2, especially 10m datasets during map transitions
- [ ] Atmosphere rim / day-night / VR
- [ ] Optional finer-than-10m country-by-country boundaries (e.g. geoBoundaries)
