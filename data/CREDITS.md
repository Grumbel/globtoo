<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Assets and licensing

- **Globtoo application source code, icons, documentation and screenshots:** GPL-3.0-or-later (see `LICENSE` and `LICENSES/GPL-3.0-or-later.txt`).
- **stb_image.h** in `external/`: Sean Barrett and contributors. The upstream header offers MIT or public-domain terms; Globtoo uses its MIT option (see `LICENSES/MIT.txt`).
- **Natural Earth country boundaries:** public domain. Downloaded by the Nix
  build and compiled into the distributed country meshes.
- **Earth imagery:** NASA Blue Marble–derived image, public domain; fetched by
  the Nix build. Credit: NASA Earth Observatory / Reto Stöckli. No placeholder Earth texture is distributed in the source tree.

The application icon was created specifically for Globtoo. Downstream
packagers should ship the icon and corresponding desktop entry together.

The AppStream metadata document is CC0-1.0 (`LICENSES/CC0-1.0.txt`).
For individual file licensing, see the SPDX headers and `REUSE.toml`.
