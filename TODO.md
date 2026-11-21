# TODO.md — globtoo

## Current tip
- GEOS triangulation of GeoJSON polygons, including concavity, holes and antimeridian
- Country fills subdivided onto sphere; depth-tested overlays hide rear hemisphere
- Fixed camera headlight sign and relocated globe UV seam to +/-180°

## Done
- [x] Document coordinate system (docs/COORDINATES.md)
- [x] Align sphere texture UVs with lon/lat, including the actual mesh seam
- [x] Political fill from MAPCOLOR7 with proper geometry, not centroid fans
- [x] Preserve depth for globe and country border/fill overlay
- [x] Geometry tests for holes, concavities, winding and antimeridian

## Next
- [ ] Atmosphere rim / day-night / VR
- [ ] Optional 10m boundaries when zoomed in
