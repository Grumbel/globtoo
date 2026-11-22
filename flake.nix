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

      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "globtoo";
          inherit version;
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            sdl3
            geos
            libGL
            libglvnd
            mesa
          ];

          cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" "-DPROJECT_VERSION_FULL=${version}" ];

          # Inject the fetched texture so the binary always has a real map
          postPatch = ''
            mkdir -p assets
            cp ${earthTexture} assets/earth.jpg
            cp ${countries50m} assets/countries_50m.geojson
            cp ${countries10m} assets/countries_10m.geojson
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin $out/share/globtoo/{assets,shaders}
            cp globtoo $out/bin/globtoo-bin
            cp assets/earth.jpg $out/share/globtoo/assets/
            cp generated-assets/countries_50m.gmesh $out/share/globtoo/assets/
            cp generated-assets/countries_10m.gmesh $out/share/globtoo/assets/
            cp -r ${./shaders}/* $out/share/globtoo/shaders/ 2>/dev/null || true

            # Thin wrapper so the binary finds its assets
            cat > $out/bin/globtoo << WRAP
            #!${pkgs.runtimeShell}
            export GLOBE_ASSET_DIR="$out/share/globtoo"
            cd "$out/share/globtoo"
            exec $out/bin/globtoo-bin "\$@"
            WRAP
            chmod +x $out/bin/globtoo
            runHook postInstall
          '';

          meta = with pkgs.lib; {
            description = "globtoo – tactile free-spinning Earth globe (texture fetched at build time)";
            license = licenses.mit;
            platforms = platforms.linux;
          };
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake pkg-config sdl3 geos libGL libglvnd mesa
            gdb clang-tools curl
          ];
          shellHook = ''
            echo "Dev shell ready."
            echo "Texture is fetched by the flake at build time (see flake.nix earthTexture)."
            echo "For a local high-res texture without rebuilding the flake:"
            echo "  curl -L -o assets/earth.jpg \\"
            echo "    https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/world.topo.bathy.200412.3x5400x2700.jpg"
          '';
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/globtoo";
        };
      });
}
