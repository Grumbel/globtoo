# TODO.md — globtoo

## Current tip
- Documented + flake-fetched Natural Earth 110m country GeoJSON
- Prior: stars, grab fix, trackball, shift-roll, camera orbit

## Country borders (next implementation)
- [ ] Minimal GeoJSON loader → lon/lat rings
- [ ] Project rings to unit sphere; draw border lines (lifted)
- [ ] Triangulate polygons OR bake political texture; color by MAPCOLOR7 / ADM0_A3
- [ ] Toggle political vs satellite (e.g. P)
- [ ] Optional: 50m dataset when zoomed in

Data: `assets/countries.geojson` (build-time fetch). See `docs/COUNTRIES.md`.

## Open / other
- [ ] Atmosphere, day/night
- [ ] VR later
