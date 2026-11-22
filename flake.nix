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

          buildInputs = (with pkgs; [
            sdl3
            geos
            libGL
            libglvnd
            mesa
          ]) ++ sdl3PcDeps;

          cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" "-DPROJECT_VERSION_FULL=${version}" ];

          # Inject the fetched texture so the binary always has a real map
          postPatch = ''
            mkdir -p assets
            cp ${earthTexture} assets/earth.jpg
            cp ${countries50m} assets/countries_50m.geojson
            cp ${countries10m} assets/countries_10m.geojson
          '';

          # cmake --install installs binary, shared resources, launcher,
          # hicolor icon, AppStream metadata, manual and license. The binary
          # locates share/globtoo relative to itself; no cd/wrapper required.

          meta = with pkgs.lib; {
            description = "Interactive 3D Earth globe with animated 2D projections";
            license = licenses.mit;
            platforms = platforms.linux;
            mainProgram = "globtoo";
          };
        };

        devShells.default = pkgs.mkShell {
          packages = (with pkgs; [
            cmake pkg-config sdl3 geos libGL libglvnd mesa
            gdb clang-tools curl
          ]) ++ sdl3PcDeps;
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
