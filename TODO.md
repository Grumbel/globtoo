# TODO.md — globtoo

## Current tip
- Slower arcball, fixed mirrored UVs, seamless 4k texture (no Wikipedia border)
- Prior: Mat4/Quat order, arcball, texture RGBA, sphere winding

## Open / next
- [ ] Confirm mirror fix and speed feel right
- [ ] Optional: true NASA BMNG 5400² via eoimages (hash at first build)
- [ ] Optional: country borders / political overlay
- [ ] Optional: atmosphere rim, day/night, clouds
- [ ] VR later

## Notes
- UV: `1-u` on the sphere so longitude matches the map (was mirrored).
- Texture: turban/webgl-earth `2_no_clouds_4k.jpg` — seamless, no white frame.
- Drag sensitivity 0.004 rad/px, inertia vel_scale 40.
