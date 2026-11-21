{
  description = "Tactile free-spinning Earth globe - SDL3 + GLES2, ECS-style";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "globe";
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
            stb
          ];

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
          ];

          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin
            cp globe $out/bin/
            mkdir -p $out/share/globe
            cp -r ../assets $out/share/globe/ || true
            cp -r ../shaders $out/share/globe/ || true
            runHook postInstall
          '';

          meta = with pkgs.lib; {
            description = "Tactile free-spinning Earth globe";
            license = licenses.mit;
            platforms = platforms.linux;
          };
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            cmake
            pkg-config
            sdl3
            libGL
            libglvnd
            mesa
            stb
            gdb
            clang-tools
          ];
        };

        apps.default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/globe";
        };
      });
}
