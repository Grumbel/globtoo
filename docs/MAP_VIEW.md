<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Map modes and unfolding transitions

## Render architecture

The app keeps a fixed unit Earth and a rotating camera (right-handed, Y up,
east longitude -> -Z). The same GLES2 geometry is used for all five modes:

- **Globe**: perspective-projected sphere with occlusion and country depth/stencil
- **Equirectangular (Plate Carrée)**: longitude x, latitude y
- **Mercator**: longitude x, 0.65 × ln(tan(π/4 + latitude/2)); above
  ±85° the curve extends along its tangent to keep projection coordinates
  finite at the rotated poles
- **Mollweide equal-area**: solved iteratively for the auxiliary latitude
- **Hammer equal-area**: closed elliptical/oval representation of a whole Earth

The 0.65 factor applied to Mercator is a display normalization, *not* a
geographic correction. It keeps the full ±85° range within a similar initial
viewport as the other projections.

`shaders/projection.glsl` is included by each geography vertex shader. For each
vertex, it calculates the position on the sphere in camera space and the
chosen 2D projection in a parallel plane; `u_morph` interpolates the
**positions**, including Z depth. That makes the globe flatten and unfold,
rather than fading between two images. Switching flat projections interpolates
between their vertex coordinates independently of the sphere-morph parameter.
Animations use interruptible exponential interpolation in `src/map_view.cpp`.

`src/scene.cpp` supplies the same rotated geographic tangent frame to the
textured sphere, country fills, border segments, and country-name placement.
For every geographic 3D vertex, the shader computes **local longitude and
latitude in this frame**, then applies the selected map projection. As the
map center moves, the whole world is reprojected; it is **not** a translated,
mirrored or repeated north/south sheet.

Country fills and the globe's projected surface use **per-triangle anchor
vectors**; borders are rendered as independent line segments with shared
per-segment anchors. The shader unwraps each primitive's longitude about
its anchor, so a moving ±180° seam cannot stretch an otherwise small triangle
or line across the whole map. Equirectangular and Mercator are horizontally
periodic, while Mollweide and Hammer are bounded and are clipped at their
new ±180° seam. Extra longitude passes are made only when the screen reaches
that seam. The old 3D globe still uses its cheaper indexed mesh; only the
projected/transitioning globe uses the deindexed, anchored vertices.

## Flat navigation across the poles and rotation

`MapView` stores a complete orthonormal 3D tangent frame after the first pan.
Each regular drag applies a great-circle rotation to **all three frame axes**,
with the local projection derivative determining the angular travel. This
avoids the pole singularity of storing a map location as only Euler
longitude/latitude. North/south navigation crosses the poles naturally:
once on the other side, geography appears from the opposite direction because
the entire Earth was rotated before it was reprojected. Sideways panning at
the pole also works. There are no mirrored polar bands or duplicate countries.

The globe and flat maps still have intrinsic topological differences. A global
2D projection cannot be completely free of cuts or distortion; the
reprojection puts its cut opposite the current viewing center. Mercator's
finite continuation above ±85° is intentionally not conformal at the pole.

Shift+left-drag rotates the flat map around its center. Left-drag
reprojects the entire Earth, applying inverse roll to geographic movement
so navigation crosses either pole. **Middle-drag** instead translates the
projected 2D sheet in the screen plane, without changing the geographic
center or the projection frame. This works at any zoom and orientation;
for cylindrical projections the horizontal translation wraps continuously.
The graticule and distance ruler follow the location beneath screen center,
and folding a panned map back into the globe centers that visible location.
Globe↔map mode switches preserve the map roll and geographic orientation.
On the 3D globe, middle-drag rolls the view; Shift+middle-drag rolls a
flat map rather than panning its sheet. North Lock suppresses deliberate
roll gestures until switched off.

## Compass, North-up and Reset

The upper-right compass rotates its N/E/S/W markings and north-pointing
needle to show **geographic north at the center of the view**. In the 3D
sphere, direction comes from the latitude-increasing tangent in the globe's
right-handed geographic frame projected into camera-right and camera-up
coordinates; in flat maps it follows the rotated world-north tangent in the current
3D map frame, accounting for any Shift+drag screen roll. The bearing is clockwise from screen-up, so
`N 000 DEG` indicates North-up. Near the geographic poles, the compass uses
the current meridian as a reference because true north is undefined exactly
at a pole.

Click **NORTH UP** (or the compass rose) to straighten the current map or
globe without moving the center or changing zoom. North-up rotates the screen to the actual local north tangent. Returning to
the globe preserves that bearing and geographical center through the fold. **RESET**, below the compass instead of in the toolbar,
restores the timezone-derived center, default zoom, and North-up orientation
without changing the chosen projection. The **GRAB / ARCBALL** button
below Reset selects the 3D globe mouse mode; `G` toggles it from the keyboard.
**NORTH LOCK** keeps geographic north pointing upward automatically as the
map or globe moves; **NORTH UP** remains an independent one-shot action.
The map toolbar is anchored to the bottom of the window, and the scale bar
is right aligned immediately beneath the compass.

Press **Tab** to hide or show the UI controls, compass, and scale bar. The
geographic grid, country names, and markers remain visible, and mouse drags
work even where the hidden controls were previously located.

UI hit testing uses the same viewport-relative bounds as drawing, so compass clicks cannot initiate a
map drag, and map labels are kept out of both control panels.

In spherical mode, depth is written only by the base globe and a stencil mask
prevents visible political geometry outside the globe limb. During flattening
we disable the silhouette mask, and in completed flat mode depth occlusion is
turned off. UI labels and controls are overlaid in 2D after geography.

## Label generation

Build-time `country_mesh_compiler` reads `NAME_EN` with a fallback to `NAME`,
finds a GEOS `PointOnSurface` anchor on each feature's largest valid polygon,
and stores that label in `.gmesh` **version 2**. At runtime labels are rendered
with a small built-in 5×7 font and filtered for visibility/collision. Labels
are uppercase and common Latin diacritics are transliterated to their ASCII
base letter. Glyphs outside the basic bitmap repertoire fall back to `?`.
Precompiled `.gmesh` v1 is still readable, but it has no names and its original
3D-only vertices don't retain unwrapped longitude for seam-safe map rendering.
Regenerate assets using the current CMake/Nix build to get labels and map
coordinates.

## Timezone at startup

`TZ`, `/etc/localtime` and `/etc/timezone` are examined in that order. For an
IANA timezone, the system's `zone1970.tab` or `zone.tab` gives a representative
city location. If it cannot be resolved, only the UTC offset is used (15° per
hour, latitude 0°). The timezone identifies a civil time region, not the
actual physical position of the user.

## Known limitations

- No native runtime GUI/font dependency is required. Country label glyphs are
  intentionally compact instead of using full Unicode outline fonts.
- A longitude seam can briefly appear during the sphere-to-map morph; extra
  longitude passes are applied only in completed flat mode to avoid ghost globes.
- The 2D map projections are rendered with GLES2 vertex geometry, not
  high-resolution raster map tiles. Very close views may still need an
  optional finer-than-Natural-Earth-10m source.
- Large globe-to-map changes reveal the whole far hemisphere during the fold;
  this is a geometric deformation, not a physical unfolding of a seamless
  spherical surface into a plane (which is mathematically impossible without
  cutting/distortion).
