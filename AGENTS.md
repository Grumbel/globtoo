# AGENTS.md — globtoo

## What this is
Tactile free-spinning Earth globe for Linux (SDL3 + GLES2).
Data-oriented / ECS-ish: plain structs + free functions, no class hierarchies.

## Standing rules
- Prefer correct design over quick hacks.
- Texture upload must always force RGBA (or set GL_UNPACK_ALIGNMENT=1) — RGB 24-bit skews.
- Sphere winding must be CCW from outside so GL_CULL_FACE + GL_BACK is correct.
- Earth texture is fetched at build time by the Nix flake; do not commit large binary textures.
- License: keep existing (MIT for the code we wrote; NASA public domain for the texture).

## Layout
- `src/` — C++ sources (camera, mesh, texture, shader, globe, input, main)
- `shaders/` — GLES2 vertex/fragment
- `flake.nix` — build + texture fetch
- `TODO.md` — current tip and open work (read this first on session start)

## Handover
Deliverables are cumulative git bundles only. See user instructions for naming and authorship.
