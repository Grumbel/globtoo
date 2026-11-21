# TODO.md — globtoo

## Current tip
- Arcball controls (quat, no pole lock), faster spin, upright texture
- Previous: texture RGBA fix + sphere winding CCW

## Open / next
- [ ] Confirm north is up and arcball feels natural across the poles
- [ ] Optional: higher-detail sphere / LOD when zoomed in
- [ ] Optional: country borders / political overlay (vector) — original “plain old globe” goal
- [ ] Optional: atmosphere rim, day/night, clouds
- [ ] VR later

## Known good
- RGBA texture upload (no 24-bit skew)
- Sphere winding CCW from outside
- Nix flake fetches texture at build time
- App name: globtoo

## Notes
- Orientation is a quaternion on the globe (model); camera sits on +Z.
- Drag builds view-space angular velocity for inertia; grab clears it.
- Texture: no vertical flip — equirectangular north (file top) → sphere v=0 (north).
