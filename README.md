# Globe

Tactile free-spinning Earth globe for Linux.

- SDL3 + OpenGL ES 2.0
- Data-oriented / ECS-ish (plain structs + free functions, no class hierarchies)
- Inertia-based spin that feels like turning a real globe
- Smooth zoom down to individual small countries
- Starts with a public-domain NASA Blue Marble style texture

## Build (Nix)

```bash
nix build
./result/bin/globe
```

Or enter a development shell:

```bash
nix develop
mkdir build && cd build
cmake ..
make -j
./globe
```

## Texture

Place an equirectangular Earth texture at `assets/earth.jpg`.

Recommended free sources (public domain / NASA):

- Blue Marble Next Generation (NASA) – various resolutions
- Solar System Scope Earth day map (CC-BY, NASA derived)

A 2k–4k JPEG works well. Higher resolution improves the close-up experience.

## Controls

- Left-drag : spin the globe (with inertia)
- Mouse wheel : zoom in / out
- Esc : quit

## Architecture notes

Plain C++20 structs + free functions. No deep inheritance.
Components live as data; systems are free functions that operate on them.
Ready for later expansion (country borders, VR, etc.).
