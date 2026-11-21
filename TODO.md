# TODO.md — globtoo

## Current tip
- Branch: master
- Last work: fix texture 24/32-bit skew + reverse sphere winding (inside-out Earth)
- Base of this stack: 887419f (GitHub tip when this fix was applied)

## Open / next
- [ ] Confirm texture looks correct after RGBA force + unpack alignment
- [ ] Confirm outside of sphere is visible (winding fixed)
- [ ] Optional: higher-detail sphere or LOD when zoomed in
- [ ] Optional: country borders / political overlay (vector) — original “plain old globe” goal
- [ ] Optional: atmosphere rim, day/night, clouds
- [ ] VR later (user mentioned)

## Known good
- Inertia drag + zoom feel
- Nix flake fetches texture at build time
- App name: globtoo

## Notes for next agent
- Do not paper over texture or culling bugs; root causes were format/alignment and winding.
- Previous bundle (001 vs 0e9c44f) targeted a tip that was rewritten on the remote; this stack starts at 887419f.
