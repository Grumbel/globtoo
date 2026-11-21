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

## Texture

The flake fetches a public-domain equirectangular Earth map at build time.

To switch to the full NASA Blue Marble Next Generation (5400×2700, topography + bathymetry):

1. Open `flake.nix`
2. Comment out the current `earthTexture` and uncomment the NASA one
3. `nix build` once – it will print the correct `sha256`
4. Paste the hash and rebuild

NASA URLs are public domain. Credit: NASA Earth Observatory / Reto Stöckli.

## Controls

- Left-drag : spin the globe (with inertia)
- Mouse wheel : zoom in / out
- Tab : switch Grab ↔ Arcball
- Shift+drag : roll around the view axis
- T : trackball mode (mouse grabbed, move to orbit)
- Space : toggle shading
- Esc : leave trackball, or quit
- Esc : quit

**Grab** (default): the surface under the cursor stays under the cursor.
**Arcball**: free view-space rotation, no pole lock.

## Architecture notes

Plain C++20 structs + free functions. No deep inheritance.
Components live as data; systems are free functions that operate on them.
Ready for later expansion (country borders, VR, etc.).


## Country data

Natural Earth Admin-0 **1:50m and 1:10m** countries (public domain) are
fetched and converted into binary GPU-ready meshes at build time. The viewer
switches to 10m for close zoom, with hysteresis to avoid LOD flicker. No
GeoJSON parsing or triangulation is needed at launch in the Nix package.
GEOS (>= 3.10) triangulates polygons, including holes; a depth-tested
spherical overlay hides the far hemisphere. See `docs/COUNTRIES.md`.

To work on the geometry without SDL3/GLES installed:

```bash
cmake -S . -B build-geometry -DGLOBTOO_BUILD_APP=OFF
cmake --build build-geometry -j
ctest --test-dir build-geometry --output-on-failure
```

Run the geometry tests using `ctest --test-dir build --output-on-failure`.
