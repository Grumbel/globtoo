# TODO.md — globtoo

## Current tip
- Camera orbits fixed Earth; stars fixed in world (real bright-star positions)
- Prior: Grab/Arcball, shading, texture/winding/arcball fixes

## Model
- Earth is fixed in world space (identity model).
- Camera orientation quaternion places the eye; controls orbit the camera.
- Stars: ~50–60 brightest catalog stars as points; view translation stripped so they sit at infinity. Rotate with the camera orbit (not with a spinning Earth).

## Open / next
- [ ] Confirm grab still feels right after camera-orbit switch
- [ ] Optional: denser catalog or milky-way band
- [ ] Optional: country borders / political overlay
- [ ] Optional: atmosphere, day/night
- [ ] VR later
