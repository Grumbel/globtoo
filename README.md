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

Natural Earth 1:110m Admin-0 countries (public domain) is fetched at build time
as GeoJSON for borders and political coloring. See `docs/COUNTRIES.md`.
