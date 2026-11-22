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

        # Real planetary rasters. Full/default builds prepare and install all
        # three; lite builds do not fetch or process any planetary imagery.
        # Fixed-output hashes come from published source/integrity records:
        #   Moon/Mars: https://github.com/RTF22/Orrery/blob/master/ASSETS.md
        #   Io: Wikimedia Commons File:Io_for_GeoHacks.jpg (SHA-1 imageinfo)
        # These pins identify the bytes; scientifically exact registration still
        # requires comparison with planetary control-network maps.
        pythonWithPillow = pkgs.python3.withPackages (ps: [ ps.pillow ps.numpy ]);
        moonRaster = pkgs.fetchurl {
          url = "https://www.solarsystemscope.com/textures/download/8k_moon.jpg";
          hash = "sha256-0Ydbzsg1iMol5IAuV29rufiLOeHkA8tB/1WGdBnFR5Y=";
          name = "moon-8k.jpg";
        };
        marsRaster = pkgs.fetchurl {
          url = "https://www.solarsystemscope.com/textures/download/8k_mars.jpg";
          hash = "sha256-TMUhSZJKvGrlB9YwMvmU4dQqVcuCwJ4ALRpWf/ZsI+4=";
          name = "mars-8k.jpg";
        };
        ioOriginal = pkgs.fetchurl {
          url = "https://upload.wikimedia.org/wikipedia/commons/4/48/Io_for_GeoHacks.jpg";
          sha1 = "8349d9424be6570cccda1d7ab3e85a82daf557cf";
          name = "io-usgs.jpg";
        };
        # Commons' USGS original has 1225x613 pixels (one pixel short of an
        # exact 2:1 grid). The raster compiler requires 2:1 cell sampling.
        # Resample the width by just 0.08%; retain the original upstream pin
        # and cite this geometric adjustment in the installed source record.
        ioRaster = pkgs.runCommand "globtoo-io-plate-carree.jpg" {
          nativeBuildInputs = [ pythonWithPillow ];
        } ''
          python3 - <<'PYTHON'
          import os
          from PIL import Image
          with Image.open("${ioOriginal}") as source:
              if source.size != (1225, 613):
                  raise SystemExit(f"unexpected USGS Io mosaic size: {source.size}")
              source.convert("RGB").resize((1226, 613), Image.Resampling.LANCZOS).save(
                  os.environ["out"], format="JPEG", quality=95, subsampling=0)
          PYTHON
        '';

        # All published imagery sources are global, north-up, plate carrée.
        # Longitude conventions are explicit; they are not inferred by the
        # converter. Solar System Scope maps are render-oriented (CC BY 4.0),
        # not authoritative geographic control-network basemaps.
        planetaryRasters = {
          moon = {
            source = moonRaster;
            url = "https://www.solarsystemscope.com/textures/download/8k_moon.jpg";
            license = "SolarSystemScope-CC-BY-4.0-display-mosaic";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          mars = {
            source = marsRaster;
            url = "https://www.solarsystemscope.com/textures/download/8k_mars.jpg";
            license = "SolarSystemScope-CC-BY-4.0-display-mosaic";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          io = {
            source = ioRaster;
            url = "https://commons.wikimedia.org/wiki/File:Io_for_GeoHacks.jpg";
            license = "USGS-NASA-public-domain";
            positive = "west"; left = "0"; latitude = "planetocentric";
          };
        };

        # A supplied audited map replaces the corresponding bundled source;
        # a caller does not have to build, install or manually copy assets.
        # Passing {} uses all bundled sources in the full build.
        mkPackage = { lite ? false, planetaryMaps ? {},
                      includePlanetaryTextures ? !lite }:
          let
            bodies = [ "moon" "mars" "io" ];
            # Only selected assets are fetched; lite has no planetary fetches
            # unless an audited map was explicitly supplied by the caller.
            activeBodies = builtins.filter (body:
              builtins.hasAttr body planetaryMaps || includePlanetaryTextures) bodies;
            bodyFlags = pkgs.lib.concatMap (body:
              let
                upper = pkgs.lib.strings.toUpper body;
              in
                if builtins.hasAttr body planetaryMaps then [
                  "-DGLOBTOO_${upper}_PREPARED_MAP=${builtins.getAttr body planetaryMaps}"
                ] else if includePlanetaryTextures then
                  let image = builtins.getAttr body planetaryRasters;
                  in [
                    "-DGLOBTOO_${upper}_IMAGERY_TEXTURE=${image.source}"
                    "-DGLOBTOO_${upper}_LONGITUDE_POSITIVE=${image.positive}"
                    "-DGLOBTOO_${upper}_LONGITUDE_LEFT=${image.left}"
                    "-DGLOBTOO_${upper}_LATITUDE_TYPE=${image.latitude}"
                    "-DGLOBTOO_${upper}_SOURCE=${image.url}"
                    "-DGLOBTOO_${upper}_LICENSE=${image.license}"
                  ]
                else []
            ) bodies;
          in pkgs.stdenv.mkDerivation {
          pname = if lite then "globtoo-lite" else "globtoo";
          inherit version;
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
            (if activeBodies != [] then pythonWithPillow else python3)
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
          ] ++ bodyFlags ++ pkgs.lib.optionals (!lite) [
            "-DGLOBTOO_EARTH_TEXTURE=${earthTexture}"
          ];

          doCheck = true;
          checkPhase = ''
            runHook preCheck
            ctest --output-on-failure
            runHook postCheck
          '';

          postPatch = ''
            mkdir -p assets
            cp ${countries50m} assets/countries_50m.geojson
          '' + pkgs.lib.optionalString (!lite) ''
            cp ${countries10m} assets/countries_10m.geojson
            cp ${places10m} assets/places_10m.geojson
            cp ${rivers10m} assets/rivers_10m.geojson
          '';

          # A manifest/preview existence check is not enough: a single bad
          # tile or mismatched base texture can silently break close zoom.
          # Validate the actual installed JPEGs, entire tile grid, attribution
          # and (for prepared maps) recorded SHA-256 before admitting the
          # output into the Nix store. No runtime Pillow dependency.
          postInstall = pkgs.lib.optionalString (activeBodies != []) ''
            PYTHONPATH=${./.} python3 ${./tools/verify_packaged_planets.py} \
              "$out" ${pkgs.lib.concatMapStringsSep " " (body: "--body ${body}") activeBodies}
          '';

          meta = with pkgs.lib; {
            description = "Interactive 3D Earth and planetary globe with animated 2D projections";
            # The optional bundled Solar System Scope mosaics are CC BY 4.0;
            # the USGS Io map is public domain.
            license = [ licenses.gpl3Plus ] ++
              pkgs.lib.optionals includePlanetaryTextures [ licenses.cc-by-40 ];
            platforms = platforms.linux;
            mainProgram = "globtoo";
          };
        };

      in
      {
        packages.default = mkPackage { };
        packages.globtoo-lite = mkPackage { lite = true; };
        # Full geographic data and Earth image without planetary tile assets.
        packages.globtoo-earth = mkPackage { includePlanetaryTextures = false; };

        # Reproducible opt-in package composition for downstream Nix flakes.
        # Input maps must be locally prepared/pinned store paths; CMake
        # verifies their source.json, manifest and every output SHA-256.
        lib.withPlanetaryMaps = planetaryMaps: mkPackage { inherit planetaryMaps; };
        lib.withPlanetaryMapsLite = planetaryMaps: mkPackage {
          lite = true; inherit planetaryMaps;
        };

        # Full and lite builds participate in nix flake check.
        checks.fat-build = self.packages.${system}.default;
        checks.lite-build = self.packages.${system}.globtoo-lite;
        # The default fat-build check covers the three packaged planetary maps.
        # globtoo-earth is an optional reduced-data package, not another check.

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
            echo "Default builds fetch Earth, Moon, Mars and Io textures at pinned hashes."
            echo "globtoo-lite omits bundled imagery; globtoo-earth keeps only Earth imagery."
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
        apps.globtoo-earth = {
          type = "app";
          program = "${self.packages.${system}.globtoo-earth}/bin/globtoo";
        };
      });
}
