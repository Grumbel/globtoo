<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# AGENTS.md — globtoo

## What this is
Tactile free-spinning Earth globe for Linux (SDL3 + GLES2).
Data-oriented / ECS-ish: plain structs + free functions, no class hierarchies.

## Standing rules
- Prefer correct design over quick hacks.
- Texture upload must always force RGBA (or set GL_UNPACK_ALIGNMENT=1) — RGB 24-bit skews.
- Sphere winding must be CCW from outside so GL_CULL_FACE + GL_BACK is correct.
- Earth texture is fetched at build time by the Nix flake; do not commit large binary textures.
- Licensing: project code and artwork are GPL-3.0-or-later. Keep per-file SPDX notices and `reuse lint` passing. `external/stb_image.h` stays MIT, AppStream metadata CC0-1.0, fetched Earth imagery and Natural Earth data are public domain.

## Layout
- `src/` — C++ sources (camera, mesh, texture, shader, globe, input, main)
- `external/` — vendored third-party headers (`stb_image.h`), not in `src/`
- `shaders/` — GLES2 vertex/fragment
- `flake.nix` — build + texture fetch
- `TODO.md` — current tip and open work (read this first on session start)

## Handover
Deliverables are cumulative git bundles only. See user instructions for naming and authorship.
