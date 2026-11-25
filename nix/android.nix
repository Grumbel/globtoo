# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{ pkgs, version, versionCode, src, resources, appFlags }:

let
  api = "21";
  compileApi = "33";
  buildTools = "30.0.3";
  ndkVersion = "29.0.14206865";
  sdk = (pkgs.androidenv.composeAndroidPackages {
    platformVersions = [ compileApi ];
    buildToolsVersions = [ buildTools ];
    includeNDK = true;
    inherit ndkVersion;
    includeEmulator = false;
    includeSources = false;
  }).androidsdk;
in pkgs.stdenvNoCC.mkDerivation {
  pname = "globtoo-android";
  inherit version src;
  nativeBuildInputs = [ pkgs.cmake pkgs.ninja pkgs.jdk17 pkgs.zip ];
  dontUseCmakeConfigure = true;
  buildPhase = ''
    runHook preBuild
    export ANDROID_HOME=${sdk}/libexec/android-sdk
    ndk="$ANDROID_HOME/ndk/${ndkVersion}"
    bt="$ANDROID_HOME/build-tools/${buildTools}"
    android_jar="$ANDROID_HOME/platforms/android-${compileApi}/android.jar"
    sdl_source=${pkgs.sdl3.src}

    for abi in armeabi-v7a arm64-v8a x86 x86_64; do
      cmake -S . -B "build-$abi" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ndk/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-${api} -DANDROID_STL=c++_static \
        ${pkgs.lib.escapeShellArgs appFlags} -DGLOBTOO_SDL_SOURCE="$sdl_source" \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_ANDROID_JAR=OFF
      cmake --build "build-$abi" --target globtoo --parallel "$NIX_BUILD_CORES"
      mkdir -p "apk/lib/$abi"
      cp "build-$abi/libmain.so" "build-$abi/sdl/libSDL3.so" "apk/lib/$abi/"
      "$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip" "apk/lib/$abi/"*.so
    done

    mkdir -p classes apk/res/drawable
    cp assets/globtoo-icon.png apk/res/drawable/icon.png
    javac -encoding UTF-8 -source 8 -target 8 -bootclasspath "$android_jar" -d classes \
      "$sdl_source"/android-project/app/src/main/java/org/libsdl/app/*.java \
      nix/android/GlobtooActivity.java
    "$bt/d8" --min-api ${api} --lib "$android_jar" --output apk $(find classes -name '*.class')

    substitute nix/android/AndroidManifest.xml apk/AndroidManifest.xml \
      --subst-var-by minSdk ${api} \
      --subst-var-by version ${pkgs.lib.escapeShellArg version} \
      --subst-var-by versionCode ${toString (pkgs.lib.max 1 versionCode)}
    cp -r ${resources} apk/assets
    chmod u+w apk/assets
    cp -r LICENSES apk/assets/
    cp data/CREDITS.md apk/assets/
    "$bt/aapt" package -f -M apk/AndroidManifest.xml -S apk/res -A apk/assets \
      -I "$android_jar" -F "$PWD/unsigned.apk"
    (cd apk && zip -qr0 ../unsigned.apk classes.dex lib)
    "$bt/zipalign" -f -p 4 unsigned.apk aligned.apk

    # A reproducible development key keeps APK upgrades compatible.
    sed '/^#/d' nix/android/debug.keystore.base64 | base64 -d > debug.keystore
    "$bt/apksigner" sign --ks debug.keystore --ks-pass pass:android --key-pass pass:android \
      --out globtoo.apk aligned.apk
    "$bt/apksigner" verify --min-sdk-version ${api} globtoo.apk
    runHook postBuild
  '';
  installPhase = ''
    install -Dm644 globtoo.apk "$out/globtoo.apk"
  '';
}
