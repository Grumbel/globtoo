<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Assets and licensing

- **Globtoo application source code, icons, documentation and screenshots:** GPL-3.0-or-later (see `LICENSE` and `LICENSES/GPL-3.0-or-later.txt`).
- **stb_image.h** in `external/`: Sean Barrett and contributors. The upstream header offers MIT or public-domain terms; Globtoo uses its MIT option (see `LICENSES/MIT.txt`).
- **Natural Earth boundaries, populated places, and rivers:** public domain.
  Downloaded by the Nix build and converted into the distributed country
  meshes and compact point/line feature asset.
- **Earth imagery:** NASA Blue Marble–derived image, public domain; fetched by
  the Nix build. Credit: NASA Earth Observatory / Reto Stöckli. No placeholder Earth texture is distributed in the source tree.
- **Optional relief/bathymetry imagery:** NASA Blue Marble Next Generation
  topography and bathymetry visualization (public domain), credited to NASA
  Earth Observatory / Reto Stöckli, using GEBCO ocean bathymetry. The high-res
  source is user supplied at build time; the source JPEG is not installed.


The application icon was created specifically for Globtoo. Downstream
packagers should ship the icon and corresponding desktop entry together.

The AppStream metadata document is CC0-1.0 (`LICENSES/CC0-1.0.txt`).
For individual file licensing, see the SPDX headers and `REUSE.toml`.
