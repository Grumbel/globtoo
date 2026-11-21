# Country borders & coloring

## Data (public domain)

**Natural Earth** Admin-0 countries — free for any use, no attribution required
(attribution appreciated).

| Scale | Use | Approx size | Source |
|-------|-----|-------------|--------|
| **1:110m** | Default / overview | ~820 KB GeoJSON | [ne_110m_admin_0_countries.geojson](https://cdn.jsdelivr.net/gh/nvkelso/natural-earth-vector@master/geojson/ne_110m_admin_0_countries.geojson) |
| **1:50m** | Medium zoom | larger | same repo `ne_50m_…` |
| **1:10m** | Close-up | multi-MB | same repo / official zip |

Official site: https://www.naturalearthdata.com/downloads/110m-cultural-vectors/

S3 zip (shapefile): `https://naturalearth.s3.amazonaws.com/110m_cultural/ne_110m_admin_0_countries.zip`

The flake fetches **110m GeoJSON** at build time into
`share/globtoo/assets/countries.geojson` (and `assets/` for local builds).

### Geometry

- CRS: WGS84 lon/lat (CRS84): coordinates `[lon, lat]` in degrees.
- Features: `Polygon` or `MultiPolygon`.
- Useful properties: `NAME`, `ADM0_A3` / `ISO_A3`, `MAPCOLOR7`/`MAPCOLOR13`
  (Natural Earth’s cartographic palette indices).

## Projection onto the unit sphere

```
lon_rad = lon * π/180
lat_rad = lat * π/180
x = cos(lat) * cos(lon)
y = sin(lat)
z = cos(lat) * sin(lon)
```

Match existing mesh UVs (`1-u` for longitude if needed for texture alignment).

## Rendering plan

1. **Borders** — line strips from ring coordinates, lifted slightly
   (`position *= 1.002`) to avoid z-fighting; GLES2 line or thin triangle strips.
2. **Country fill / political colors** — either:
   - **A.** Ear-clip / constrained triangulation of each polygon → colored meshes
     with a flat color from `MAPCOLOR7` or a hash of `ADM0_A3`; or
   - **B.** Offline bake a political equirectangular texture from the same
     GeoJSON (simpler GPU path, less crisp at extreme zoom).
3. Toggle political layer vs satellite texture (e.g. key `P`).

Prefer **A** for the classic multi-colored desk globe look at moderate
detail (110m); switch to 50m when zoomed if needed.

## Parser notes

- Minimal GeoJSON: scan for `"coordinates"` arrays; no full JSON library
  required if we keep a tiny hand parser or use nlohmann/json via Nix.
- Skip antimeridian-split artifacts carefully; Natural Earth already splits
  most problematic polygons.
