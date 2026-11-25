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
        # Packages with Earth tiles use the tile derivation's own base.jpg
        # (same source and colours as the close-up tiles). This stylized 4k
        # day map (turban/webgl-earth) is only the fallback for a build
        # without tiles; it is also the lite downloader's pinned asset.
        earthTextureFallback = pkgs.fetchurl {
          url = "https://raw.githubusercontent.com/turban/webgl-earth/master/images/2_no_clouds_4k.jpg";
          sha256 = "sha256-Yc09J0idgO0CXXlUU5ptkLUNRDO73Sfowa+KbxX3HoY=";
          name = "earth.jpg";
        };

        # True-color Earth imagery: NASA Blue Marble Next Generation, July
        # 2004, with topography and bathymetry shading (public domain; NASA
        # Earth Observatory / Reto Stöckli). The plain bmng-base variant has
        # uniformly black oceans; here the shaded sea floor is recoloured
        # lighter by the tiler (--recolor brighten-ocean). All zoom levels,
        # including the 4096-pixel distant/morph base, come from this source.
        bmngBase = "https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-topography-bathymetry/july";
        bmngLicense = "Public domain (NASA Earth Observatory, Blue Marble Next Generation)";
        bmngRaster = tag: file: sha256: pkgs.fetchurl {
          url = "${bmngBase}/${file}";
          inherit sha256;
          name = "earth-bmng-${tag}.jpg";
        };
        # 21600x10800 (1 arc-minute, ~1.9 km at the equator): default package.
        bmng21600 = [
          (bmngRaster "21600" "world.topo.bathy.200407.3x21600x10800.jpg"
            "1c57d7r89wv41s03a2mphl04fgj3yd46xppnv38s8j34bbrz29fj")
        ];
        # 86400x43200 (15 arc-seconds, ~460 m) as eight 21600x21600
        # quadrants: A..D are 90-degree columns from 180W eastwards, 1 is the
        # northern and 2 the southern half. Listed row-major for the tiler's
        # --mosaic 4x2. globtoo-max only.
        bmng86400 = map ({ tile, sha256 }:
          bmngRaster tile "world.topo.bathy.200407.3x21600x21600.${tile}.jpg" sha256) [
          { tile = "A1"; sha256 = "0w6bdchi6j4r93mdkq3hz3n8581z78xbq1d6jpdagiw0l6si18yd"; }
          { tile = "B1"; sha256 = "13jm51xl535rww3cd5kkgxg5nrnx3ryj2wndzpkv7rknasswxn0m"; }
          { tile = "C1"; sha256 = "182s8xg1cb9a4gkq1nzdwi8b1h1l6adydn0sil6n4pdk3smr117f"; }
          { tile = "D1"; sha256 = "0mlvc2kf5bdp5prxng00bbakra3icwsk5738vh587798k170lj27"; }
          { tile = "A2"; sha256 = "00md4pk3lxlms5qshk5n3bkiz7hbx0j70nbf9mqab9kljybnc2qz"; }
          { tile = "B2"; sha256 = "1mw6rm5kjfqcir6z8fnrykypscgadj3i83513pmq2dq9zx4lrkip"; }
          { tile = "C2"; sha256 = "02v6lb5snvdpg0a3h1da2hiwdr9yvsv9gxydswaspsnqdqffcdrj"; }
          { tile = "D2"; sha256 = "19by4d29cw95dp71n3avwxb4sjdx6msxz8rnqgh6b08v2k8c50v8"; }
        ];

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

        # Real planetary rasters for every body in data/bodies.tsv except
        # Earth. Full/default builds prepare and install all of them; lite
        # builds do not fetch or process any planetary imagery.
        # Fixed-output hashes come from published source/integrity records:
        #   Solar System Scope: https://github.com/RTF22/Orrery/blob/master/ASSETS.md
        #     and SRI digests recorded when the pins were added
        #   Wikimedia Commons: the SHA-1 published by the imageinfo API
        # These pins identify the bytes; scientifically exact registration still
        # requires comparison with planetary control-network maps.
        pythonWithPillow = pkgs.python3.withPackages (ps: [ ps.pillow ps.numpy ]);
        # Sources above 250 megapixels (globtoo-max only) are tiled through
        # libvips; default builds never pull it in.
        pythonWithVips = pkgs.python3.withPackages (ps: [ ps.pillow ps.numpy ps.pyvips ]);
        sssLicense = "SolarSystemScope-CC-BY-4.0-display-mosaic";
        sssRaster = file: hash: pkgs.fetchurl {
          url = "https://www.solarsystemscope.com/textures/download/${file}";
          inherit hash;
          name = "sss-${file}";
        };
        commonsRaster = body: url: sha1: pkgs.fetchurl {
          inherit url sha1;
          name = "${body}-commons.${pkgs.lib.last (pkgs.lib.splitString "." url)}";
        };
        # Some published mosaics are a pixel or two off an exact 2:1 grid.
        # Resample the width (< 0.1%) with the source size pinned, so a
        # changed upstream file fails instead of being silently re-gridded.
        # Such entries set `preprocessing`, which CMake writes into the
        # installed source record next to the original URL.
        fitPlateCarree = body: source: expect: pkgs.runCommand
          "globtoo-${body}-plate-carree.jpg" {
            nativeBuildInputs = [ pythonWithPillow ];
          } ''
            python3 ${./tools/fit_plate_carree.py} ${source} "$out" --expect ${expect}
          '';

        # Every source is a global plate carrée raster. Longitude direction,
        # left boundary and top pole are declared per source and verified by
        # tools/planetary_alignment.py QA sheets against IAU feature centers;
        # nothing is inferred by the converter. Solar System Scope maps are
        # render-oriented (CC BY 4.0), not authoritative control-network
        # basemaps; the Commons maps are NASA/JPL/USGS public-domain mosaics.
        # Gas giant maps are cloud-top textures with no fixed surface frame.
        planetaryRasters = {
          mercury = {
            source = sssRaster "8k_mercury.jpg" "sha256-XIvYha41cca6LNNLNEa5xtdn4xS/DujB1cFHyt04j8M=";
            url = "https://www.solarsystemscope.com/textures/download/8k_mercury.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          venus = {
            source = sssRaster "8k_venus_surface.jpg" "sha256-m8IaUFd+2KxzTNqRBYckx6dBwZQnqidiJM40k1FDLFs=";
            url = "https://www.solarsystemscope.com/textures/download/8k_venus_surface.jpg";
            license = sssLicense;
            positive = "west"; left = "180"; top = "south"; latitude = "planetocentric";
          };
          moon = {
            source = sssRaster "8k_moon.jpg" "sha256-0Ydbzsg1iMol5IAuV29rufiLOeHkA8tB/1WGdBnFR5Y=";
            url = "https://www.solarsystemscope.com/textures/download/8k_moon.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          mars = {
            source = sssRaster "8k_mars.jpg" "sha256-TMUhSZJKvGrlB9YwMvmU4dQqVcuCwJ4ALRpWf/ZsI+4=";
            url = "https://www.solarsystemscope.com/textures/download/8k_mars.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          jupiter = {
            source = sssRaster "8k_jupiter.jpg" "sha256-C9hEvyCCLE4+gIgrB3hZgzwNrETH5OHgzWPRsbbUMIU=";
            url = "https://www.solarsystemscope.com/textures/download/8k_jupiter.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          # Commons' USGS original is 1225x613, one pixel short of 2:1.
          # Despite the west-positive Gazetteer coordinates of Io's features,
          # this raster is stored east-positive with 180 deg at its left edge:
          # Pele's plume ring, Loki and Tvashtar only register that way.
          io = {
            source = fitPlateCarree "io" (commonsRaster "io"
              "https://upload.wikimedia.org/wikipedia/commons/4/48/Io_for_GeoHacks.jpg"
              "8349d9424be6570cccda1d7ab3e85a82daf557cf") "1225x613";
            url = "https://commons.wikimedia.org/wiki/File:Io_for_GeoHacks.jpg";
            license = "USGS-NASA-public-domain";
            preprocessing = "width resampled from 1225x613 to exact 2:1 by tools/fit_plate_carree.py";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          europa = {
            source = commonsRaster "europa"
              "https://upload.wikimedia.org/wikipedia/commons/7/74/Jupiter_II-Europa_map_NASA_JPL_Voyager.jpg"
              "b34dd701296c18cfcf4787a55a6e508a327a9615";
            url = "https://commons.wikimedia.org/wiki/File:Jupiter_II-Europa_map_NASA_JPL_Voyager.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          ganymede = {
            source = commonsRaster "ganymede"
              "https://upload.wikimedia.org/wikipedia/commons/b/b3/Ganymede_map_NASA_JPL_Voyager.jpg"
              "18a10b0ded2939f892869f0a704b01e01ed3b027";
            url = "https://commons.wikimedia.org/wiki/File:Ganymede_map_NASA_JPL_Voyager.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          callisto = {
            source = commonsRaster "callisto"
              "https://upload.wikimedia.org/wikipedia/commons/b/b2/Callisto_map_NASA_JPL_Voyager.jpg"
              "e4cf460a601ce9f22e963f4d7ca2a5b49a5fe583";
            url = "https://commons.wikimedia.org/wiki/File:Callisto_map_NASA_JPL_Voyager.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          saturn = {
            source = sssRaster "8k_saturn.jpg" "sha256-DTmkpJDIfD7avgCjiBopuzQYNkF4x5xTT+CYbpfgmFM=";
            url = "https://www.solarsystemscope.com/textures/download/8k_saturn.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          mimas = {
            source = commonsRaster "mimas"
              "https://upload.wikimedia.org/wikipedia/commons/4/4f/Map_of_Mimas_colorized_2014-04_PIA18437.jpg"
              "cd0ee6757fd429569548ba451b72a6fb047b9d85";
            url = "https://commons.wikimedia.org/wiki/File:Map_of_Mimas_colorized_2014-04_PIA18437.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          enceladus = {
            source = commonsRaster "enceladus"
              "https://upload.wikimedia.org/wikipedia/commons/a/aa/Enceladus_Color_Map.jpg"
              "f0bf6d306bd14fd8f955cf70f0f3762a634e7c4c";
            url = "https://commons.wikimedia.org/wiki/File:Enceladus_Color_Map.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          tethys = {
            source = fitPlateCarree "tethys" (commonsRaster "tethys"
              "https://upload.wikimedia.org/wikipedia/commons/4/40/Tethys_Color_Map.jpg"
              "54ae75ef0f93eeff2fad0dc5cca6a9696e79391c") "13467x6734";
            url = "https://commons.wikimedia.org/wiki/File:Tethys_Color_Map.jpg";
            license = "NASA-public-domain";
            preprocessing = "width resampled from 13467x6734 to exact 2:1 by tools/fit_plate_carree.py";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          dione = {
            source = commonsRaster "dione"
              "https://upload.wikimedia.org/wikipedia/commons/1/18/Dione_Color_Map.jpg"
              "882d49df52d51d62bbbe1394f906dbb88d137a67";
            url = "https://commons.wikimedia.org/wiki/File:Dione_Color_Map.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          rhea = {
            source = fitPlateCarree "rhea" (commonsRaster "rhea"
              "https://upload.wikimedia.org/wikipedia/commons/2/21/Rhea_Color_Map.jpg"
              "29d011d85adcd68014f6166b9f24212549465e2b") "12015x6008";
            url = "https://commons.wikimedia.org/wiki/File:Rhea_Color_Map.jpg";
            license = "NASA-public-domain";
            preprocessing = "width resampled from 12015x6008 to exact 2:1 by tools/fit_plate_carree.py";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          titan = {
            source = commonsRaster "titan"
              "https://upload.wikimedia.org/wikipedia/commons/7/7a/Titan_map_April_2011_full.png"
              "3970174250a7a0bfcbdbb589b93a032bb5de24c8";
            url = "https://commons.wikimedia.org/wiki/File:Titan_map_April_2011_full.png";
            license = "NASA-public-domain";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          iapetus = {
            source = fitPlateCarree "iapetus" (commonsRaster "iapetus"
              "https://upload.wikimedia.org/wikipedia/commons/5/53/Iapetus_Color_Map.jpg"
              "a86d7ab38dbcd0154c3baa0f1f1f4f56a1f5f638") "11741x5871";
            url = "https://commons.wikimedia.org/wiki/File:Iapetus_Color_Map.jpg";
            license = "NASA-public-domain";
            preprocessing = "width resampled from 11741x5871 to exact 2:1 by tools/fit_plate_carree.py";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          uranus = {
            source = sssRaster "2k_uranus.jpg" "sha256-0VI51G+C0+oT0rJgtbKbKjgvQvKRba4GlNA4exIEoJ0=";
            url = "https://www.solarsystemscope.com/textures/download/2k_uranus.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          miranda = {
            source = commonsRaster "miranda"
              "https://upload.wikimedia.org/wikipedia/commons/8/81/Miranda_map_JPL_USGS.jpg"
              "3c044872df3047effa0aa46ab56d2eb17a5dc961";
            url = "https://commons.wikimedia.org/wiki/File:Miranda_map_JPL_USGS.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          ariel = {
            source = commonsRaster "ariel"
              "https://upload.wikimedia.org/wikipedia/commons/7/74/Ariel_map_JPL_USGS.jpg"
              "0b053acb5295d4eb049d46a76e71c1a956c026b4";
            url = "https://commons.wikimedia.org/wiki/File:Ariel_map_JPL_USGS.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          umbriel = {
            source = commonsRaster "umbriel"
              "https://upload.wikimedia.org/wikipedia/commons/4/42/Umbriel_map_JPL_USGS.jpg"
              "ff07ceb497e35902bc72a9267a2c8a1bd1ac1c5e";
            url = "https://commons.wikimedia.org/wiki/File:Umbriel_map_JPL_USGS.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          titania = {
            source = commonsRaster "titania"
              "https://upload.wikimedia.org/wikipedia/commons/8/85/Titania_map_JPL_USGS.jpg"
              "71f9c89e0b2e67ebef9b2342832284351fa42fdc";
            url = "https://commons.wikimedia.org/wiki/File:Titania_map_JPL_USGS.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          oberon = {
            source = commonsRaster "oberon"
              "https://upload.wikimedia.org/wikipedia/commons/1/1d/Oberon_map_JPL_USGS.jpg"
              "6a3561f6eff9125c0d5b1b5ce0adde29dc826f8b";
            url = "https://commons.wikimedia.org/wiki/File:Oberon_map_JPL_USGS.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          neptune = {
            source = sssRaster "2k_neptune.jpg" "sha256-y0LqgnCXQdKLCvRNiyg8vG29DFIafw4eHgEK3gCXffY=";
            url = "https://www.solarsystemscope.com/textures/download/2k_neptune.jpg";
            license = sssLicense;
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          triton = {
            source = commonsRaster "triton"
              "https://upload.wikimedia.org/wikipedia/commons/6/61/Triton_map_no_grid.jpg"
              "a3ece50cd073f166e1faaa4950a5d3676dc00d1f";
            url = "https://commons.wikimedia.org/wiki/File:Triton_map_no_grid.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
          pluto = {
            source = commonsRaster "pluto"
              "https://upload.wikimedia.org/wikipedia/commons/a/ad/Pluto_color_mapmosaic.jpg"
              "bb6726dc5c8d52443ffc9d071c55c49ce1b0cdaa";
            url = "https://commons.wikimedia.org/wiki/File:Pluto_color_mapmosaic.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "0"; latitude = "planetocentric";
          };
          charon = {
            source = commonsRaster "charon"
              "https://upload.wikimedia.org/wikipedia/commons/3/39/Charon_map_iau1803c.jpg"
              "72b7f12128b900b0fc589e6d2081fd069a2389d8";
            url = "https://commons.wikimedia.org/wiki/File:Charon_map_iau1803c.jpg";
            license = "NASA-public-domain";
            positive = "east"; left = "180"; latitude = "planetocentric";
          };
        };

        # Viewer order, from the shared body catalog.
        bodyOrder = map (line: builtins.head (pkgs.lib.splitString "\t" line))
          (builtins.filter (line: line != "" && !(pkgs.lib.hasPrefix "#" line))
            (pkgs.lib.splitString "\n" (builtins.readFile ./data/bodies.tsv)));
        planetaryBodies = builtins.filter (body: body != "earth") bodyOrder;

        # Texture generation lives in its own derivations, one per body, so
        # app code changes never re-tile gigapixels of imagery. Their source
        # is limited to the tiling tools and catalogs they actually read.
        mapToolsSrc = pkgs.lib.fileset.toSource {
          root = ./.;
          fileset = pkgs.lib.fileset.unions [
            ./tools/prepare_planetary_map.py
            ./tools/build_raster_tiles.py
            ./tools/raster_manifest.py
            ./tools/planetary_alignment.py
            ./tools/normalize_planet_texture.py
            ./tools/planetary_bodies.py
            ./tools/planetary_latitudes.py
            ./tools/install_planetary_map.py
            ./data/bodies.tsv
            ./data/planetary_features.tsv
          ];
        };
        # Output: an audited prepared map (tiles, previews, source.json with
        # per-file SHA-256, alignment QA sheet), the same format as a manual
        # tools/prepare_planetary_map.py run and GLOBTOO_<BODY>_PREPARED_MAP.
        mkPlanetaryMap = body: image: pkgs.runCommand "globtoo-map-${body}" {
            nativeBuildInputs = [ (image.python or pythonWithPillow) ];
          } ''
            # prepare stages next to its destination; the store is read-only.
            python3 ${mapToolsSrc}/tools/prepare_planetary_map.py \
              ${image.source} "$NIX_BUILD_TOP/${body}" \
              --body ${body} --projection plate-carree \
              --source-url ${pkgs.lib.escapeShellArg image.url} \
              --license ${pkgs.lib.escapeShellArg image.license} \
              --positive ${image.positive} --left ${image.left} \
              --top ${image.top or "north"} --latitude ${image.latitude} \
              ${pkgs.lib.escapeShellArgs (pkgs.lib.optionals (image ? preprocessing)
                [ "--preprocessing" image.preprocessing ])}
            python3 ${mapToolsSrc}/tools/install_planetary_map.py \
              "$NIX_BUILD_TOP/${body}" --verify --body ${body}
            cp -r "$NIX_BUILD_TOP/${body}" "$out"
          '';
        bundledMaps = builtins.mapAttrs mkPlanetaryMap planetaryRasters;

        # globtoo-max Moon: LROC WAC global 100 m/pixel mosaic, 109164x54582
        # 8-bit grayscale, an uncompressed 6 GB BigTIFF (public domain; NASA/
        # GSFC/Arizona State University, distributed by USGS Astrogeology).
        # SimpleCylindrical, upper-left corner at 180W/90N, east-positive.
        moonMaxMap = mkPlanetaryMap "moon" {
          source = pkgs.fetchurl {
            url = "https://asc-pds-services.s3.us-west-2.amazonaws.com/mosaic/Lunar_LRO_LROC-WAC_Mosaic_global_100m_June2013.tif";
            sha256 = "0mgx6rcmf153ad9wn168grq2n0j31737x5bycjxk3bl832nrw5c7";
            name = "moon-lroc-wac-100m.tif";
          };
          url = "https://astrogeology.usgs.gov/search/map/moon_lro_lroc_wac_global_morphology_mosaic_100m";
          license = "Public domain (NASA/GSFC/Arizona State University LROC; USGS Astrogeology)";
          positive = "east"; left = "180"; latitude = "planetocentric";
          python = pythonWithVips;
        };

        # Earth close-zoom imagery, tiled once in its own derivation (like the
        # planetary maps) and passed to CMake as GLOBTOO_IMAGERY_TILES.
        mkEarthImagery = name: sources: mosaic: python:
          pkgs.runCommand "globtoo-earth-imagery-${name}" {
            nativeBuildInputs = [ python ];
          } ''
            python3 ${mapToolsSrc}/tools/build_raster_tiles.py \
              ${pkgs.lib.escapeShellArgs sources} "$out" --mosaic ${mosaic} \
              --recolor brighten-ocean --base 4096
            {
              echo "Body: earth"
              echo "Source: NASA Blue Marble Next Generation, July 2004 (${bmngBase}/)"
              echo "License: ${bmngLicense}"
              echo "Credit: NASA Earth Observatory / Reto Stöckli"
              echo "Processing: oceans recoloured lighter (build_raster_tiles.py --recolor brighten-ocean)"
              ${pkgs.lib.concatMapStrings (source: ''
                echo "Original file: ${source.url} (SHA-256 ${builtins.convertHash {
                  hash = source.outputHash; hashAlgo = "sha256"; toHashFormat = "base16"; }})"
              '') sources}
            } > "$out/source.txt"
          '';
        earthImagery = mkEarthImagery "21600" bmng21600 "1x1" pythonWithPillow;
        earthImageryMax = mkEarthImagery "86400" bmng86400 "4x2" pythonWithVips;

        # A supplied audited map replaces the corresponding bundled one;
        # a caller does not have to build, install or manually copy assets.
        # Passing {} uses all bundled maps in the full build.
        mkPackage = { lite ? false, planetaryMaps ? {},
                      includePlanetaryTextures ? !lite,
                      earthTiles ? (if lite then null else earthImagery),
                      pname ? (if lite then "globtoo-lite" else "globtoo") }:
          let
            # Only selected assets are fetched; lite has no planetary fetches
            # unless an audited map was explicitly supplied by the caller.
            maps = (if includePlanetaryTextures
                    then pkgs.lib.getAttrs planetaryBodies bundledMaps
                    else {}) // planetaryMaps;
            activeBodies = builtins.filter (body: maps ? ${body}) planetaryBodies;
            bodyFlags = map (body:
              "-DGLOBTOO_${pkgs.lib.strings.toUpper body}_PREPARED_MAP=${maps.${body}}"
            ) activeBodies;
          in pkgs.stdenv.mkDerivation {
          inherit pname version;
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
          ] ++ bodyFlags ++ (if earthTiles != null then [
            # Distant views and morphs use the tiles' own base image, so
            # zooming never switches to a differently coloured source.
            "-DGLOBTOO_EARTH_TEXTURE=${earthTiles}/base.jpg"
            "-DGLOBTOO_IMAGERY_TILES=${earthTiles}"
          ] else pkgs.lib.optionals (!lite) [
            "-DGLOBTOO_EARTH_TEXTURE=${earthTextureFallback}"
          ]);

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
            python3 ${./.}/tools/verify_packaged_planets.py \
              "$out" ${pkgs.lib.concatMapStringsSep " " (body: "--body ${body}") activeBodies}
          '';

          meta = with pkgs.lib; {
            description = "Interactive 3D Earth and planetary globe with animated 2D projections";
            # The optional bundled Solar System Scope mosaics are CC BY 4.0;
            # the NASA/JPL/USGS Commons maps are public domain.
            license = [ licenses.gpl3Plus ] ++
              pkgs.lib.optionals includePlanetaryTextures [ licenses.cc-by-40 ];
            platforms = platforms.linux;
            mainProgram = "globtoo";
          };
        };

        portablePackages = import ./nix/portable.nix {
          inherit pkgs version countries50m;
          earthTexture = earthTextureFallback;
          versionCode = self.revCount or 1;
          androidPkgs = import nixpkgs {
            inherit system;
            config.allowUnfree = true;
            config.android_sdk.accept_license = true;
          };
        };

      in
      {
        packages = {
          default = mkPackage { };
          globtoo-lite = mkPackage { lite = true; };
          # Full geographic data and Earth image without planetary tile assets.
          globtoo-earth = mkPackage { includePlanetaryTextures = false; };
          # Maximum-resolution imagery: Earth BMNG 86400x43200 and the LROC
          # WAC 100 m Moon (6 GB download, several GB of tiles). Not a check.
          globtoo-max = mkPackage {
            pname = "globtoo-max";
            earthTiles = earthImageryMax;
            planetaryMaps = { moon = moonMaxMap; };
          };
          earth-imagery = earthImagery;
          earth-imagery-max = earthImageryMax;
          planetary-map-moon-max = moonMaxMap;
          # All bundled prepared planetary maps; each body is also exposed as
          # `planetary-map-<body>`. Built independently of the app, so code
          # changes reuse them from the store instead of re-tiling.
          planetary-maps = pkgs.linkFarm "globtoo-planetary-maps"
            (pkgs.lib.mapAttrsToList (name: path: { inherit name path; }) bundledMaps);
        } // pkgs.lib.mapAttrs' (body: prepared:
          pkgs.lib.nameValuePair "planetary-map-${body}" prepared) bundledMaps
        // pkgs.lib.optionalAttrs (system == "x86_64-linux") portablePackages;

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
        # The default fat-build check covers every packaged planetary map.
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
            python3Packages.rasterio python3Packages.pyvips
            sdl3 geos libGL libglvnd mesa reuse
            gdb clang-tools curl openssl
          ]) ++ sdl3PcDeps;
          shellHook = ''
            echo "Dev shell ready."
            echo "Default builds fetch Earth and all planet/moon textures at pinned hashes."
            echo "globtoo-lite omits bundled imagery; globtoo-earth keeps only Earth imagery."
            echo "Optional high-resolution Earth imagery / relief can be tiled with tools/build_raster_tiles.py."
            echo "Signed numeric ETOPO1 can be tiled with tools/build_elevation_tiles.py."
            echo "GPWv4 density can be tiled with tools/build_population_tiles.py (optional)."
            echo "See docs/RASTER_LAYERS.md (Pillow is included in this shell)."
            echo "Prebuilt Earth tiles for a local build:"
            echo "  nix build .#earth-imagery -o /tmp/globtoo-imagery"
            echo "  cmake -S . -B build -DGLOBTOO_IMAGERY_TILES=/tmp/globtoo-imagery"
          '';
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/globtoo";
          meta.description = "globtoo with Earth imagery, full geographic data and all bundled planet/moon textures";
        };
        apps.globtoo-lite = {
          type = "app";
          program = "${self.packages.${system}.globtoo-lite}/bin/globtoo";
          meta.description = "globtoo lite build without bundled imagery and with reduced geographic data";
        };
        apps.globtoo-max = {
          type = "app";
          program = "${self.packages.${system}.globtoo-max}/bin/globtoo";
          meta.description = "globtoo with maximum-resolution Earth (86400x43200) and Moon (LROC WAC 100 m) imagery";
        };
        apps.globtoo-earth = {
          type = "app";
          program = "${self.packages.${system}.globtoo-earth}/bin/globtoo";
          meta.description = "globtoo with Earth imagery and full geographic data, without planetary textures";
        };
      });
}
