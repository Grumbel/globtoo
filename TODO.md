# TODO.md — globtoo

## Current tip
- Fixed grab-mode feedback (stable surface drag)
- Prior: trackball, shift-roll, stars, camera orbit

## Grab math
- grab_prev is the fixed world hit from mouse-down for the whole drag
- Each move: rotate camera by conjugate of (a→b) so Earth appears to follow
- Per-frame angle clamped; soft inertia (no angle/dt)

## Open / next
- [ ] Optional: denser stars, borders, atmosphere, VR
