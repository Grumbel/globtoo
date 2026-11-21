# TODO.md — globtoo

## Current tip
- Grab control mode (surface under cursor) + Arcball via Tab
- Optional Lambert shading toggle with Space
- Prior: speed/mirror/texture, Mat4 order, arcball, RGBA, winding

## Open / next
- [ ] Confirm grab feels locked to the surface at different zoom levels
- [ ] Optional: NASA BMNG higher res
- [ ] Optional: country borders / political overlay
- [ ] Optional: atmosphere rim, day/night, clouds
- [ ] VR later

## Controls
- Tab — Grab ↔ Arcball
- Space — shading on/off
- Drag / wheel / Esc as before

## Notes
- Grab uses ray–sphere hits; rotation axis = cross(prev, curr).
- Default mode is Grab; shading defaults on.
