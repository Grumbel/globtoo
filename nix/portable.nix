# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{ pkgs, androidPkgs, version, versionCode, countries50m, earthTexture }:

let
  inherit (pkgs) lib;
  src = lib.cleanSource ../.;
  meshes = pkgs.stdenv.mkDerivation {
    pname = "globtoo-country-meshes";
    inherit version src;
    nativeBuildInputs = [ pkgs.cmake ];
    buildInputs = [ pkgs.geos ];
    cmakeFlags = [ "-DGLOBTOO_BUILD_APP=OFF" "-DBUILD_TESTING=OFF" ];
    postPatch = ''
      cp ${countries50m} assets/countries_50m.geojson
    '';
    buildPhase = ''
      runHook preBuild
      cmake --build . --target country_meshes --parallel "$NIX_BUILD_CORES"
      runHook postBuild
    '';
    installPhase = ''
      install -Dm644 generated-assets/countries_50m.gmesh "$out/countries_50m.gmesh"
    '';
  };

  resources = pkgs.runCommand "globtoo-portable-resources" {} ''
    mkdir -p "$out/assets"
    cp -r ${../shaders} "$out/shaders"
    cp ${../assets/globtoo-icon.png} "$out/assets/globtoo-icon.png"
    cp ${earthTexture} "$out/assets/earth.jpg"
    cp ${meshes}/countries_50m.gmesh "$out/assets/"
  '';

  appFlags = [
    "-DPROJECT_VERSION_FULL=${version}"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_TESTING=OFF"
    "-DGLOBTOO_PRECOMPILED_COUNTRIES=ON"
    "-DGLOBTOO_COUNTRY_MESH_DIR=${meshes}"
    "-DGLOBTOO_EARTH_TEXTURE=${earthTexture}"
  ];

  mkWindows = arch: cross:
    let
      sdl = cross.sdl3;
      threads = cross.windows.mcfgthreads;
      app = cross.stdenv.mkDerivation {
        pname = "globtoo-${arch}-build";
        inherit version src;
        nativeBuildInputs = [ pkgs.cmake ];
        buildInputs = [ sdl threads ];
        cmakeFlags = appFlags ++ [ "-DCMAKE_EXE_LINKER_FLAGS=-static-libgcc -static-libstdc++" ];
      };
    in pkgs.runCommand "globtoo-${version}-${arch}" {} ''
      mkdir -p "$out"
      cp -r ${app}/{bin,share} "$out/"
      chmod u+w "$out/bin"
      cp ${lib.getBin sdl}/bin/SDL3.dll "$out/bin/"
      find ${lib.getBin threads}/bin -name 'libmcfgthread-[0-9]*.dll' \
        -exec cp -L {} "$out/bin/" \;
      test -s "$out/bin/globtoo.exe"
    '';

  wasm = pkgs.stdenv.mkDerivation {
    pname = "globtoo-wasm";
    inherit version src;
    nativeBuildInputs = [ pkgs.emscripten pkgs.cmake pkgs.ninja pkgs.python3 ];
    dontUseCmakeConfigure = true;
    configurePhase = ''
      runHook preConfigure
      export EM_CACHE="$TMPDIR/emcache"
      cp -r ${pkgs.emscripten}/share/emscripten/cache "$EM_CACHE"
      chmod -R u+w "$EM_CACHE"
      emcmake cmake -S . -B build -G Ninja -DCMAKE_INSTALL_PREFIX=/ \
        ${lib.escapeShellArgs appFlags} \
        -DGLOBTOO_SDL_SOURCE=${pkgs.sdl3.src} \
        -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF \
        -DGLOBTOO_WEB_RESOURCES=${resources}
      runHook postConfigure
    '';
    buildPhase = ''
      runHook preBuild
      cmake --build build --target globtoo --parallel "$NIX_BUILD_CORES"
      runHook postBuild
    '';
    installPhase = ''
      runHook preInstall
      mkdir -p "$out"
      cp build/globtoo.html "$out/index.html"
      cp build/globtoo.{js,wasm,data} "$out/"
      cp -r LICENSES "$out/"
      cp data/CREDITS.md "$out/"
      runHook postInstall
    '';
    dontStrip = true;
  };
  r36s = import ./r36s.nix { inherit pkgs version src appFlags; };
in {
  globtoo-win32 = mkWindows "win32" pkgs.pkgsCross.mingw-msvcrt-i686;
  globtoo-win64 = mkWindows "win64" pkgs.pkgsCross.mingw-msvcrt-x86_64;
  globtoo-wasm = wasm;
  globtoo-android = import ./android.nix {
    pkgs = androidPkgs;
    inherit version versionCode src resources appFlags;
  };
  globtoo-r36s = r36s;
} // import ./portmaster.nix { inherit pkgs version r36s; }
