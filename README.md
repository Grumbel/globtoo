# globtoo

Tactile free-spinning Earth globe (globtoo) for Linux.

- SDL3 + OpenGL ES 2.0
- Data-oriented / ECS-ish (plain structs + free functions, no class hierarchies)
- Inertia-based spin that feels like turning a real globe
- Smooth zoom down to individual small countries
- Earth texture is **fetched at build time** by the Nix flake (does not bloat the repo)

## Build (Nix)

```bash
nix build
./result/bin/globtoo
```

Or development shell:

```bash
nix develop
mkdir build && cd build
cmake ..
make -j
# optional: drop a real texture next to the binary
curl -L -o assets/earth.jpg \
  https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/world.topo.bathy.200412.3x5400x2700.jpg
# Optional: provide assets/countries_50m.geojson and/or
# assets/countries_10m.geojson, then re-run cmake to precompile their meshes.
# Legacy assets/countries.geojson is still supported as runtime fallback.
./globtoo
```

## Command-line markers and version

Markers are rendered on the globe and **all** flat map projections, including
animated transitions. Pins remain a constant pixel size when zooming; labels
are optional. Pins on the far side of the globe are hidden. Multiple `--marker`
arguments, including different formats, may be combined:

```sh
globtoo --marker '52.5200,13.4050|Berlin' \
        --marker '40°41′21″N, 74°02′40″W' --label 'Liberty Island' \
        --marker 'geo:48.8583,2.2945' --label 'Eiffel Tower' \
        --center '52.5200,13.4050'

# Colors and sizes apply to subsequent pins; --marker-lonlat uses GIS order.
globtoo --marker-color '#ff6565' --marker-size 9 \
        --marker-lonlat '13.4050,52.5200' --label 'Berlin' \
        --marker-color '#66ddff' --marker '+40.6892-074.0445/'

# CSV (latitude,longitude,label) or coordinate | label on each line:
globtoo --marker-file locations.csv

globtoo --version
globtoo --help
```

Use `--marker` / `-m` repeatedly, `--label` / `-l` for the previous pin,
`--marker-color #RRGGBB` and `--marker-size 2..32` for subsequent pins,
`--marker-file` for bulk input, `--center` for the starting viewpoint, and
`--texture` (or the legacy positional filename) for a custom Earth image.
By default the starting viewpoint still follows the local timezone.

The coordinate parser understands signed decimal degrees (latitude first),
`N/S/E/W` directional prefixes or suffixes, degrees/minutes/seconds (`° ' "`
or `:` separators), degrees/decimal minutes, `geo:latitude,longitude` URIs
(RFC 5870; altitude accepted but not used), and ISO 6709 signed pairs such as
`+5230.5+01324.5/`. It also accepts whitespace-separated coordinates and
`--marker-lonlat` for longitude-first GIS values. Numeric values are validated
against WGS84 latitude/longitude ranges. For examples and limitations see
[Marker CLI](docs/MARKERS.md).

The top-level `VERSION` file is the sole version source. `--version` (and the
window title) display the same full version produced by CMake or Nix. Git
`-dev` builds include the revision count and hash; releases do not.

## Texture

The flake fetches a public-domain equirectangular Earth map at build time.

To switch to the full NASA Blue Marble Next Generation (5400×2700, topography + bathymetry):

1. Open `flake.nix`
2. Comment out the current `earthTexture` and uncomment the NASA one
3. `nix build` once – it will print the correct `sha256`
4. Paste the hash and rebuild

NASA URLs are public domain. Credit: NASA Earth Observatory / Reto Stöckli.

## Controls

Globtoo now has a small on-screen toolbar; keyboard shortcuts remain optional.

- **3D Globe**: classic tactile, inertia-based sphere.
- **Plate**: equirectangular geographic map.
- **Mercator**: cylindrical conformal projection (latitude clipped at 85°).
- **Mollweide**: equal-area ellipse.
- **Hammer**: equal-area oval.
- **− / +**: zoom out / in.
- **Shading**: toggle directional shading (flat maps automatically fade to unlit).
- **Countries**: toggle the political color overlay independently of the texture.
- **Borders**: toggle national boundary outlines.
- **Names**: toggle English country names; labels are omitted when overlapping.
- **Texture**: toggle the Earth imagery independently of the country colors. When both
  are enabled (the default), translucent country colors mix with the texture
  so terrain remains visible beneath the colors. Either layer can be shown alone.
- **Grid**: toggle the adaptive geographic latitude/longitude graticule in
  all globe and map projections. Intervals get finer when zoomed in. Lines
  follow the actual geographic meridians/parallels, including polar rotation;
  these are angles and **not** equally spaced kilometer lines, particularly
  near the geographic poles.
- **Scale bar (lower-left)**: local great-circle distance measured at the
  view center, automatically displayed in km or m. The graduated bar updates
  with zoom, projection, and globe/map transitions. Its value applies at the
  **center**; on projections, distortion increases away from the center.
- **Grab mode**: switch between Grab and Arcball in globe mode.
- **Compass (upper-right)**: live geographic N/E/S/W heading for the globe and every 2D projection.
- **North Up**: click the button or compass rose to straighten the view without panning or changing zoom (also works after a polar crossing).
- **Reset**: below the compass; restore the timezone-relative viewpoint, North-up orientation, and default zoom without changing the selected projection.

Mouse: **left-drag** spins the globe or rotates/reprojects the Earth in 2D;
**middle-drag** grabs and slides the 2D map like a paper sheet (no geographic
rotation); wheel zooms. **Shift+left-drag** rolls the map around the viewport
center, just as it rolls the 3D globe. The rotation is retained when unfolding or folding.
The geographic grid and distance ruler track the point beneath screen center
after middle-drag; folding to 3D centers that same point.
Flat maps can be dragged north or south **through the poles**. Rather than
mirroring a flat map sheet, the entire Earth rotates and is reprojected around
the new geographic center. There are no mirrored north/south map copies.
Both horizontal and vertical drags rotate the spherical coordinate frame,
so navigation remains continuous even at a geographic pole.
The 3D globe now permits roughly **1.9× closer** views (minimum camera
distance 1.08 Earth radii versus 1.15 previously). Flat-map zoom reaches
**80×** instead of 60×. The globe sphere uses a finer mesh to stay smooth
at close range. The 10m country mesh is selected automatically at these scales;
the raster Earth texture still has its original resolution.
Shift+drag rolls the globe or rotates the flat map around the screen center.
Optional shortcuts: `1..5` select projection, `Space` shading, `B` borders,
`N` names, `P` political coloring, `V` texture, `Tab` Grab/Arcball, `T` trackball,
`Esc` quit (or release trackball).

The default view is centered on the **representative city of the system's IANA
TZ zone** (from the installed `zone1970.tab`/`zone.tab`). For example,
`TZ=Europe/Berlin globtoo` centers on Berlin's zone reference coordinates.
If the timezone has no geographical entry, the app uses the UTC offset as an
approximate longitude and latitude 0°. A timezone alone is not a precise
geolocation.

The **globe↔map transitions animate the same geometry** between a sphere and
flattened projected coordinates. Projection-to-projection changes are smoothly
interpolated too; this is not a crossfade between static images. See
`docs/MAP_VIEW.md` for design details and current limitations.

## Architecture notes

Plain C++20 structs + free functions. No deep inheritance.
Components live as data; systems are free functions that operate on them.
Ready for later expansion (country borders, VR, etc.).


## Country data

Natural Earth Admin-0 **1:50m and 1:10m** countries (public domain) are
fetched and converted into binary GPU-ready meshes at build time. The viewer
switches to 10m for close zoom, with hysteresis to avoid LOD flicker. Each generated mesh now stores English `NAME_EN` country labels, interior
label anchors, and geographic coordinates alongside the shared border/fill
vertices. No GeoJSON parsing or triangulation is needed at launch in the Nix package.
GEOS (>= 3.10) triangulates polygons, including holes; a depth-tested
spherical overlay hides the far hemisphere. See `docs/COUNTRIES.md`.

To work on the geometry without SDL3/GLES installed:

```bash
cmake -S . -B build-geometry -DGLOBTOO_BUILD_APP=OFF
cmake --build build-geometry -j
ctest --test-dir build-geometry --output-on-failure
```

Run the geometry tests using `ctest --test-dir build --output-on-failure`.
