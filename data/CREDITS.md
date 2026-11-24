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
- **Close-zoom Earth imagery:** NASA Blue Marble Next Generation, July 2004
  (21,600×10,800 by default; the 86,400×43,200 A1…D2 tiles in `globtoo-max`),
  public domain. Credit: NASA Earth Observatory / Reto Stöckli. Fetched at
  pinned hashes and installed only as derived tiles.
- **Moon, `globtoo-max` only:** USGS Astrogeology LRO LROC WAC Global
  Morphology Mosaic, 100 m/pixel, public domain. Credit: NASA/GSFC/Arizona
  State University. Installed only as derived tiles.
- **Saturn's rings:** generated procedurally from published ring and gap
  radii; no third-party ring imagery is used.
- **Optional relief/bathymetry imagery:** NASA Blue Marble Next Generation
  topography and bathymetry visualization (public domain), credited to NASA
  Earth Observatory / Reto Stöckli, using GEBCO ocean bathymetry. The high-res
  source is user supplied at build time; the source JPEG is not installed.


The application icon was created specifically for Globtoo. Downstream
packagers should ship the icon and corresponding desktop entry together.

The AppStream metadata document is CC0-1.0 (`LICENSES/CC0-1.0.txt`).
For individual file licensing, see the SPDX headers and `REUSE.toml`.

## Bundled planetary imagery in the default Nix package

- **Moon, Mercury, Venus, Mars, Jupiter, Saturn (8K) and Uranus, Neptune
  (2K):** Solar System Scope planetary textures,
  used under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).
  Source: https://www.solarsystemscope.com/textures/ . The images include
  aesthetic processing and reconstructed/fictitious coverage; Globtoo rotates
  and tiles them for OpenGL ES 2.0 and generates lower-resolution previews.
  The Venus surface texture is published rotated by 180 degrees (south up);
  Globtoo declares that orientation and turns it north-up when tiling.
- **Io:** USGS Astrogeology Research Program Galileo/Voyager composite,
  public domain, mirrored at
  https://commons.wikimedia.org/wiki/File:Io_for_GeoHacks.jpg .
  Globtoo changes the source aspect ratio by one pixel to form an exact
  2:1 global plate-carrée grid and produces tiled, longitude-normalized output.

- **Europa, Ganymede, Callisto, Miranda, Ariel, Umbriel, Titania, Oberon:**
  Caltech/JPL/USGS Voyager–Galileo global maps (JPL Solar System Simulator),
  public domain, via Wikimedia Commons. The Uranian maps only cover the
  hemispheres imaged by Voyager 2; unimaged areas are left blank.
- **Mimas, Enceladus, Tethys, Dione, Rhea, Iapetus:** Cassini ISS global color
  mosaics, NASA/JPL-Caltech/Space Science Institute/Lunar and Planetary
  Institute (PIA18437, PIA18435, PIA18439, PIA18434, PIA18438, PIA18436),
  public domain, via Wikimedia Commons.
- **Titan:** Cassini ISS global map, NASA/JPL/Space Science Institute
  (PIA14908, April 2011), public domain.
- **Triton:** Voyager 2 global mosaic, Paul Schenk / Lunar and Planetary
  Institute, public domain, via Wikimedia Commons.
- **Pluto and Charon:** New Horizons global mosaics, NASA/Johns Hopkins
  University Applied Physics Laboratory/Southwest Research Institute
  (Charon as mirrored by the IAU, iau1803c), public domain.

Where a published mosaic is a pixel short of an exact 2:1 grid (Io, Tethys,
Rhea, Iapetus), Globtoo resamples its width to form an exact global
plate-carrée grid; every map is longitude-normalized and tiled. Exact
source URLs and hashes are pinned in `flake.nix` and recorded in each
installed `imagery/<body>/source.txt`.

The files are downloaded at build time, not checked into the source tree.
These acknowledgments do not imply affiliation or endorsement by NASA, JPL, USGS, LPI,
JHUAPL, SwRI, the IAU, Solar System Scope or Wikimedia Commons.
