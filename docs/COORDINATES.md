# Coordinate systems

## World / sphere (right-handed, Y-up)

| Geographic | Cartesian |
|------------|-----------|
| lon 0°, lat 0° | **+X** |
| lon 90°E | **−Z** |
| lon 90°W | **+Z** |
| lat 90°N | **+Y** |
| lat 90°S | **−Y** |

```
x = cos(lat) * cos(lon)
y = sin(lat)
z = -cos(lat) * sin(lon)
```
(`lon` / `lat` in radians.) The **minus on Z is intentional**. From a camera looking at Greenwich with north up, the screen-right direction is −Z; increasing east longitude must also move right. The old +Z longitude mapping mirrored the globe.

**Single source of truth:** `src/geocoordinates.hpp` provides `geo::lonlat_to_xyz`, `geo::xyz_to_lonlat` and `geo::uv_to_xyz`. `mesh.cpp` (sphere vertices), `country_geometry.cpp` (all fill/border vertices), and `stars.cpp` (celestial RA/Dec directions) use the same transform. Do not duplicate trigonometric conversions.

## Equirectangular texture UVs

Standard plate carrée (NASA / Natural Earth style):

- **u = 0** at 180°W, **u = 0.5** at lon 0°, **u = 1** at 180°E
- **v = 0** at north pole, **v = 1** at south pole

```
longitude = (u - 0.5) * 360°
latitude  = 90° - v * 180°
position  = geo::lonlat_to_xyz(longitude, latitude)
```

The sphere winding must be **CCW as viewed from outside** (with increasing u now heading toward −Z, the south/east triangle indices must follow that handedness). The longitude texture seam is a mesh seam at ±180°, not a polygon crossing the UV discontinuity.

`stbi_set_flip_vertically_on_load(0)` — file top (north) stays at v=0 because stb_image's first decoded row is uploaded as GL's first row (v=0).

## Camera and interaction

Earth is fixed in world space. The camera orientation quaternion orbits the eye on a sphere around the origin. The view matrix is right-handed (`look_at` with `right = forward × up`); grab ray casting and inertia use this same camera-space frame. Stars use the same view rotation without translation.

The unit-radius globe uses a tight, distance-dependent depth range in `Camera::proj_matrix`. Stars are drawn with depth testing disabled, using their own long-range projection. The globe depth buffer and an 8-bit visible-disc stencil mask are preserved while country fills and border lines are drawn. The political overlay tests against the globe depth but does **not** write to it, avoiding self-clipping and biased z-fighting near the horizon.

## Checks

Run `coordinates_test` for geographic basis directions, texture seam, round-trip conversion, sphere winding, screen east/west ordering, and camera depth range. Run `country_geometry_test` for country fill/border correspondence and geographic alignment.
