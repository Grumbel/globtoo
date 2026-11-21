# Map modes and unfolding transitions

## Render architecture

The app keeps a fixed unit Earth and a rotating camera (right-handed, Y up,
east longitude -> -Z). The same GLES2 geometry is used for all five modes:

- **Globe**: perspective-projected sphere with occlusion and country depth/stencil
- **Equirectangular (Plate Carrée)**: longitude x, latitude y
- **Mercator**: longitude x, 0.65 × ln(tan(π/4 + latitude/2)); above
  ±85° the curve extends along its tangent to give the otherwise infinite
  poles a finite coordinate in the wrapped navigation sheet
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

`src/scene.cpp` sets the same projection uniforms for the textured sphere,
country fill triangles and border strips. Country meshes retain **unwrapped
longitude/latitude coordinates** in addition to original 3D positions. This
is essential to project the 180° seam without making long triangles across
continents. In completed flat mode, longitude-periodic mesh copies are rendered when
needed at the antimeridian, with the displayed ±180° range clipped by the
fragment shaders. The seam test accounts for the longitude shift of each
pole-reflected band even at close zoom.
The transitional intermediate uses one mesh copy, so seams can temporarily
show at the far edge of the unfolding surface.

## Flat navigation across the poles and rotation

Latitude is stored as a continuous *unwrapped* angular coordinate. It is not
clamped to ±85° when panning. The map is rendered in successive north/south
bands: geographic latitude alternately reflects around the poles and
longitude shifts 180° with each crossing. The inverse of this mapping is
used when returning from the map to the 3D globe. All layers (textured
surface, country fills, outlines, labels) use the same bands, so colors and
borders remain registered. Only the bands intersecting the visible viewport
are drawn. Longitude and latitude center coordinates are periodically
normalized to keep shader arguments numerically bounded.

The coordinate transforms have to distort or cut somewhere: no flat
projection can cover the sphere seamlessly. In particular, Plate Carrée has
a polar edge along which reflected copies need not match point-for-point;
Mollweide and Hammer meet at a projection tip. This is a convenient,
geographically correct navigation convention, not an undistorted global
continuous map. The finite Mercator polar continuation is not conformal
above ±85°.

Shift+left-drag rotates the flat map around its center. Regular dragging
applies the inverse roll before updating the map's geographic center,
allowing panning in any rotated orientation. Globe↔map mode switches preserve
this roll and the true geographic center even after crossing the poles.

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
- A longitude seam may be briefly exposed while a sphere is unfolding; the
  three periodic copies are used only at the end to avoid ghost globes.
- The 2D map projections are rendered with GLES2 vertex geometry, not
  high-resolution raster map tiles. Very close views may still need an
  optional finer-than-Natural-Earth-10m source.
- Large globe-to-map changes reveal the whole far hemisphere during the fold;
  this is a geometric deformation, not a physical unfolding of a seamless
  spherical surface into a plane (which is mathematically impossible without
  cutting/distortion).
