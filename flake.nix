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

        # ---------------------------------------------------------------
        # Earth texture (fetched at build time – does NOT bloat the repo)
        # ---------------------------------------------------------------
        # Default: public-domain equirectangular (Wikimedia)
        # Good enough for development and still looks decent when zoomed.
        earthTexture = pkgs.fetchurl {
          url = "https://upload.wikimedia.org/wikipedia/commons/8/83/Equirectangular_projection_SW.jpg";
          sha256 = "sha256-rzU8MSiP8PCIkH0tXJXZWazCWUxQtkIW9GzElMmASOk=";
          name = "earth.jpg";
        };

        # Higher-quality option (NASA Blue Marble Next Generation, public domain)
        # Uncomment and replace the earthTexture above when you want the real thing.
        # First build will fail with a hash mismatch; copy the suggested hash.
        #
        # earthTexture = pkgs.fetchurl {
        #   url = "https://eoimages.gsfc.nasa.gov/images/imagerecords/73000/73909/world.topo.bathy.200412.3x5400x2700.jpg";
        #   sha256 = "sha256-............................................";  # fill from error
        #   name = "earth.jpg";
        # };
        #
        # Other good NASA months (same resolution):
        #   world.topo.bathy.200408.3x5400x2700.jpg  (August)
        #   world.topo.bathy.200406.3x5400x2700.jpg  (June)
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "globtoo";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = with pkgs; [
            cmake
            pkg-config
          ];

          buildInputs = with pkgs; [
            sdl3
            libGL
            libglvnd
            mesa
          ];

          cmakeFlags = [ "-DCMAKE_BUILD_TYPE=Release" ];

          # Inject the fetched texture so the binary always has a real map
          postPatch = ''
            mkdir -p assets
            cp ${earthTexture} assets/earth.jpg
          '';

          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin $out/share/globtoo/{assets,shaders}
            cp globtoo $out/bin/globtoo-bin
            cp assets/earth.jpg $out/share/globtoo/assets/
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
            cmake pkg-config sdl3 libGL libglvnd mesa
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
