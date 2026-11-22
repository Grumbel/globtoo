# Assets and licensing

- **Globtoo application source code and new application icon:** MIT (see `LICENSE`).
- **stb_image.h** in `external/`: Sean Barrett and contributors, public domain
  or MIT; see the notice inside the vendored header.
- **Natural Earth country boundaries:** public domain. Downloaded by the Nix
  build and compiled into the distributed country meshes.
- **Earth imagery:** NASA Blue Marble–derived image, public domain; fetched by
  the Nix build. Credit: NASA Earth Observatory / Reto Stöckli. The tiny bundled
  placeholder `assets/earth.jpg` is only for local development.

The application icon was created specifically for Globtoo. Downstream
packagers should ship the icon and corresponding desktop entry together.
