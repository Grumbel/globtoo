# TODO.md — globtoo

## Current tip
- Correct right-handed east/west coordinates across geographic sphere, political mesh and stars
- Country overlay tests the globe depth without writing it and uses globe silhouette stencil
- Adaptive globe depth range when zoomed; stars use independent sky projection
- Third-party stb_image moved into external/

## Done
- [x] Document coordinate system (docs/COORDINATES.md)
- [x] Align sphere texture UVs with lon/lat, including the actual mesh seam
- [x] Political fill from MAPCOLOR7 with proper geometry, not centroid fans
- [x] Preserve depth for globe and country border/fill overlay
- [x] Geometry tests for holes, concavities, winding and antimeridian
- [x] Fix east/west mirroring and verify world/screen orientation
- [x] Prevent political overlay depth/stencil artifacts at globe silhouette
- [x] Move third-party headers to external/

## Next
- [ ] Atmosphere rim / day-night / VR
- [ ] Optional 10m boundaries when zoomed in
