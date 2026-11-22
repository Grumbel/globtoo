# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{
  description = "globtoo – tactile free-spinning Earth globe - SDL3 + GLES2, ECS-style";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
        versionBase = nixpkgs.lib.strings.removeSuffix "\n" (builtins.readFile ./VERSION);
        gitRev = self.dirtyShortRev or self.shortRev or "dirty";
        isDev = nixpkgs.lib.strings.hasInfix "-dev" versionBase;
        version = if isDev then
          "${versionBase}.${toString (self.revCount or 0)}+g${gitRev}"
          else versionBase;

        # pkg-config may inspect SDL3's Requires.private modules even when
        # the application itself only links SDL3, GLES2, and EGL.
        sdl3PcDeps = with pkgs; [
          alsa-lib.dev
          libjack2.dev
          pipewire.dev
          libpulseaudio.dev
          libx11.dev
          libxext.dev
          libxcb.dev
          libxdmcp.dev
          libxcursor.dev
          libxi.dev
          libxfixes.dev
          libxrandr.dev
          libxscrnsaver
          libxtst
          libdrm.dev
          libgbm
          libffi.dev
          wayland.dev
          libxkbcommon.dev
          libdecor.dev
          libusb1.dev
        ];

        # ---------------------------------------------------------------
        # Earth texture (fetched at build time – does NOT bloat the repo)
        # ---------------------------------------------------------------
        # Default: public-domain equirectangular (Wikimedia)
        # Good enough for development and still looks decent when zoomed.
        # Seamless equirectangular day map (no clouds, no white border).
        # NASA Blue Marble–derived 4k used by turban/webgl-earth.
        # For full NASA BMNG 5400x2700 (public domain), swap URL to e.g.
        #   https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/world.topo.bathy.200412.3x5400x2700.jpg
        # and update sha256 from the first failed build.
        earthTexture = pkgs.fetchurl {
          url = "https://raw.githubusercontent.com/turban/webgl-earth/master/images/2_no_clouds_4k.jpg";
          sha256 = "sha256-Yc09J0idgO0CXXlUU5ptkLUNRDO73Sfowa+KbxX3HoY=";
          name = "earth.jpg";
        };

        # Natural Earth Admin-0 countries (public domain). The 50m mesh is
        # used at a distance; 10m is switched in when zooming closer. Both
        # are triangulated into binary meshes at build time, not on launch.
        countries50m = pkgs.fetchurl {
          url = "https://cdn.jsdelivr.net/gh/nvkelso/natural-earth-vector@master/geojson/ne_50m_admin_0_countries.geojson";
          sha256 = "sha256-PkWPwDatCmZBHywebKxJxde/uByxEjvFE7IlEaK3/es=";
          name = "ne_50m_admin_0_countries.geojson";
        };

        # Natural Earth 5.1.2, fixed to the upstream commit and hash.
        countries10m = pkgs.fetchurl {
          url = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/ca96624a56bd078437bca8184e78163e5039ad19/geojson/ne_10m_admin_0_countries.geojson";
          sha256 = "sha256-I57sV6wX8QChHiU2z/xWdSwxi1CudlsJGP96q0zo8lU=";
          name = "ne_10m_admin_0_countries.geojson";
        };

        # Natural Earth 5.1.2 populated places (simplified properties) and
        # 10m river centerlines; source data stays in the build sandbox.
        places10m = pkgs.fetchurl {
          url = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/ca96624a56bd078437bca8184e78163e5039ad19/geojson/ne_10m_populated_places_simple.geojson";
          hash = "sha256-/T+oZ6Mgy9XFtrtbxVCv7sKTn7LO9ojlCABygqVaxC8=";
          name = "ne_10m_populated_places_simple.geojson";
        };
        rivers10m = pkgs.fetchurl {
          url = "https://raw.githubusercontent.com/nvkelso/natural-earth-vector/ca96624a56bd078437bca8184e78163e5039ad19/geojson/ne_10m_rivers_lake_centerlines.geojson";
          hash = "sha256-u4VKkA7L07QI30bV4W4+D5dLpVmT+di1wm6FUnPAkFo=";
          name = "ne_10m_rivers_lake_centerlines.geojson";
        };

        # Downstream flakes can pass verified, already prepared tile trees
        # without adding arbitrary network fetches or fake source hashes here.
        mkPackage = { lite ? false, planetaryMaps ? {} }: pkgs.stdenv.mkDerivation {
          pname = if lite then "globtoo-lite" else "globtoo";
          inherit version;
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            python3
          ];

          buildInputs = (with pkgs; [
            sdl3
            geos
            libGL
            libglvnd
            mesa
          ]) ++ sdl3PcDeps ++ pkgs.lib.optionals lite [ pkgs.curl pkgs.openssl ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DPROJECT_VERSION_FULL=${version}"
            "-DGLOBTOO_ENABLE_DOWNLOADS=${if lite then "ON" else "OFF"}"
            # Install the pinned, fetched image itself; never a source-tree
            # placeholder or a file that happened to be named earth.jpg.
          ] ++ pkgs.lib.concatMap (body:
            pkgs.lib.optional (builtins.hasAttr body planetaryMaps)
              "-DGLOBTOO_${pkgs.lib.strings.toUpper body}_PREPARED_MAP=${builtins.getAttr body planetaryMaps}"
          ) [ "moon" "mars" "io" ] ++ pkgs.lib.optionals (!lite) [
            "-DGLOBTOO_EARTH_TEXTURE=${earthTexture}"
            # Optional high-resolution true-color tiles are supported by
            # -DGLOBTOO_IMAGERY_TEXTURE=<verified path> from a downstream
            # derivation; do not add a fetch with a fabricated hash here.
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';

          # Only GeoJSON is needed at source/configure time (mesh generation).
          # The image is installed directly from its immutable fetchurl path.
          postPatch = ''
            mkdir -p assets
            cp ${countries50m} assets/countries_50m.geojson
          '' + pkgs.lib.optionalString (!lite) ''
            cp ${countries10m} assets/countries_10m.geojson
            cp ${places10m} assets/places_10m.geojson
            cp ${rivers10m} assets/rivers_10m.geojson
          '';

          # cmake --install installs binary, shared resources, launcher,
          # hicolor icon, AppStream metadata, manual and license. The binary
          # locates share/globtoo relative to itself; no cd/wrapper required.

          meta = with pkgs.lib; {
            description = "Interactive 3D Earth globe with animated 2D projections";
            license = licenses.gpl3Plus;
            platforms = platforms.linux;
            mainProgram = "globtoo";
          };
        };

      in
      {
        packages.default = mkPackage { };
        packages.globtoo-lite = mkPackage { lite = true; };

        # Reproducible opt-in package composition for downstream Nix flakes.
        # Input maps must be locally prepared/pinned store paths; CMake
        # verifies their source.json, manifest and every output SHA-256.
        lib.withPlanetaryMaps = planetaryMaps: mkPackage { inherit planetaryMaps; };
        lib.withPlanetaryMapsLite = planetaryMaps: mkPackage {
          lite = true; inherit planetaryMaps;
        };

        # Both build variants participate in nix flake check.
        checks.fat-build = self.packages.${system}.default;
        checks.lite-build = self.packages.${system}.globtoo-lite;

        # Keep licensing checks part of `nix flake check` on every system.
        checks.reuse = pkgs.runCommand "globtoo-reuse-lint" {
          src = ./.;
          nativeBuildInputs = [ pkgs.reuse ];
        } ''
          cd "$src"
          reuse lint
          touch "$out"
        '';

        devShells.default = pkgs.mkShell {
          packages = (with pkgs; [
            cmake pkg-config python3 python3Packages.pillow python3Packages.numpy
            python3Packages.rasterio
            sdl3 geos libGL libglvnd mesa reuse
            gdb clang-tools curl openssl
          ]) ++ sdl3PcDeps;
          shellHook = ''
            echo "Dev shell ready."
            echo "Default imagery is fetched by the flake at build time (earthTexture)."
            echo "Optional high-resolution Earth imagery / relief can be tiled with tools/build_raster_tiles.py."
            echo "Signed numeric ETOPO1 can be tiled with tools/build_elevation_tiles.py."
            echo "GPWv4 density can be tiled with tools/build_population_tiles.py (optional)."
            echo "See docs/RASTER_LAYERS.md (Pillow is included in this shell)."
            echo "For a local high-res texture without rebuilding the flake:"
            echo "  curl -L -o /tmp/globtoo-earth.jpg \\"
            echo "    https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/world.topo.bathy.200412.3x5400x2700.jpg"
            echo "  cmake -S . -B build -DGLOBTOO_EARTH_TEXTURE=/tmp/globtoo-earth.jpg"
          '';
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/globtoo";
        };
        apps.globtoo-lite = {
          type = "app";
          program = "${self.packages.${system}.globtoo-lite}/bin/globtoo";
        };
      });
}
