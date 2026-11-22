<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Packaging and release checklist

Globtoo's top-level `VERSION` file is the **sole version source**.
Keep `x.y.z-dev` in development; CMake and the Nix flake derive the full
`x.y.z-dev.<revCount>+g<hash>` identifier automatically. A clean checkout and
a dirty build can have different Git metadata; packaging with
`-DPROJECT_VERSION_FULL=...` overrides the result without changing `VERSION`.

## Package layout

The CMake `install` target is the canonical installer for both ordinary Linux
builds and the Nix flake. Installed files include:

- `bin/globtoo`: normal ELF executable, not a wrapper script
- `share/globtoo/assets/`: the explicitly configured real Earth texture (when present), icon, and precompiled 50m/10m country meshes
- `share/globtoo/shaders/`: GLES2 shader source files
- `share/applications/io.github.Grumbel.globtoo.desktop`: desktop launcher
- `share/metainfo/io.github.Grumbel.globtoo.metainfo.xml`: AppStream metadata
- `share/icons/hicolor/{32,48,64,128,256}x*/apps/globtoo.png`: desktop icons
- `share/man/man1/globtoo.1`: manual, configured from `VERSION`
- `share/doc/globtoo/`: README, technical documentation, GPL-3.0-or-later license, third-party license texts and asset credits

Compiled `.gmesh` files are installed instead of the much larger original
GeoJSON files. The build needs those original data files to regenerate the
compiled meshes; the Nix derivation fetches both and pins their hashes.

The Earth image is **not tracked in Git**. The Nix derivation fetches its
pinned image and passes that store path directly as
`-DGLOBTOO_EARTH_TEXTURE=...`; the CMake installer renames that file to
`share/globtoo/assets/earth.jpg`. It must not copy a placeholder from
`assets/`. For standalone CMake builds, pass the same option with an actual
texture file (or use `--texture` at runtime). Without it, no `earth.jpg` is
installed and rendering falls back to a solid-color Earth base.

The executable finds assets beside a build-tree binary, or relative to its
installation `bin` directory (no `chdir` needed); `GLOBE_ASSET_DIR` takes
precedence. If installing under a nonstandard directory hierarchy, the CMake
configured resource path is a final fallback.

## Quick checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DGLOBTOO_EARTH_TEXTURE=/path/to/real-earth-texture.jpg
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /tmp/globtoo-stage
/tmp/globtoo-stage/bin/globtoo --version
(cd /tmp && /tmp/globtoo-stage/bin/globtoo --help)

# On systems with these optional validators:
desktop-file-validate /tmp/globtoo-stage/share/applications/*.desktop
appstreamcli validate /tmp/globtoo-stage/share/metainfo/*.xml
```

Test `globtoo` from outside the build/install directory as well: the Earth
texture, shader includes, both mesh LODs, and the window icon should load.
Confirm that `share/globtoo/assets/earth.jpg` is byte-identical to the
configured fetched source and is not the removed placeholder.
Confirm that a `.desktop` launch shows the Globtoo icon on your desktop and
that keyboard and mouse controls work in 3D and 2D.

The geometry tests can be built without SDL3 using
`-DGLOBTOO_BUILD_APP=OFF`. This doesn't validate the graphical runtime or
Nix's pinned asset downloads.

Run `reuse lint` before release (`nix develop -c reuse lint` or
`nix flake check` also work);
this validates per-file SPDX notices and the vendored MIT license.

## Release steps

1. Change `VERSION` from `x.y.z-dev` to `x.y.z` and commit it.
2. Build and test the release. `--version` and the man page must show exactly
   `x.y.z` (no hash or revision number).
3. Tag the release commit `vx.y.z` (for example `v0.2.0`). The tag and `VERSION`
   must match, and the tag must target the release version commit.
4. Distribute the built package / Git tag, and verify the icon, manual, and
   desktop integration on at least one real SDL3/GLES2 Linux desktop.
5. On the development branch, bump `VERSION` to the next `-dev` number.

Do not use Git archive substitution or hard-code a second package version.
