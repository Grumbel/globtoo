# Coordinate systems

## World / sphere (right-handed, Y-up)

| Geographic | Cartesian |
|------------|-----------|
| lon 0°, lat 0° | **+X** |
| lon 90°E | **+Z** |
| lon 90°W | **−Z** |
| lat 90°N | **+Y** |
| lat 90°S | **−Y** |

```
x = cos(lat) * cos(lon)
y = sin(lat)
z = cos(lat) * sin(lon)
```
(`lon` / `lat` in radians.)

Natural Earth and borders/fills use this via `lonlat_to_xyz`.

## Equirectangular texture UVs

Standard plate carrée (NASA / Natural Earth style):

- **u = 0** at 180°W, **u = 0.5** at lon 0°, **u = 1** at 180°E  
- **v = 0** at north pole, **v = 1** at south pole  

Sphere mesh sets:

```
theta = t * 2π          // t = 0..1, theta = lon with 0 at +X
u = fract(t + 0.5)      // so +X (lon 0) → u = 0.5
v = stack / stacks      // 0 at north
```

`stbi_set_flip_vertically_on_load(0)` — file top (north) stays at v=0.

## Camera

Earth is fixed in world space. The camera orientation quaternion orbits the eye on a sphere around the origin. Stars use the same view rotation with translation stripped.

## Checklist if borders look mirrored vs texture

1. Confirm texture is true equirectangular (not a weird crop).  
2. Mesh UVs must use `fract(t + 0.5)`, **not** `1 - t`.  
3. `lonlat_to_xyz` must match the table above (do not negate lon).  
