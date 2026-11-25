# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

# Build and ship SDL3 against the Ubuntu Eoan ABI used by ArkOS.
{ pkgs, version, src, appFlags }:

let
  lib = pkgs.lib;

  # eoan arm64 packages: glibc, the kernel headers, GCC 9's runtime and
  # libstdc++ headers, and the headers (with the libraries they name) SDL3's
  # KMS/DRM, EGL/GLES, ALSA and udev code is built against; SDL3 loads those
  # libraries from the device at run time.
  debs = [
    { file = "pool/main/g/glibc/libc6_2.30-0ubuntu2_arm64.deb"; sha256 = "4853da1e3c9eae109708b30dc5df00b7ced888c09f0e77bf6731015a97fb859a"; }
    { file = "pool/main/g/glibc/libc6-dev_2.30-0ubuntu2_arm64.deb"; sha256 = "a0b1860c16c50934a5fe3206ac386267d24b015b0ab677005d38e60ac1d27823"; }
    { file = "pool/main/l/linux/linux-libc-dev_5.3.0-18.19_arm64.deb"; sha256 = "7575a37b1abf722f950fa8202c49f24b9c887dcf8e1f97bad15b555cb8c483de"; }
    { file = "pool/main/g/gcc-9/libgcc1_9.2.1-9ubuntu2_arm64.deb"; sha256 = "81dfdee2c50f13dab98967b0062e51f1cebb967f407e3593c5a77ed2146ea008"; }
    { file = "pool/main/g/gcc-9/libstdc++6_9.2.1-9ubuntu2_arm64.deb"; sha256 = "5af692b4ecf237979411acf74b7319785e09dd3450a917a94f3ea65cd260436e"; }
    { file = "pool/main/g/gcc-9/libstdc++-9-dev_9.2.1-9ubuntu2_arm64.deb"; sha256 = "aa0efe55ba5002e2d3572271b6c0106dfb59f7a6c61be0a0c406c4836e12472b"; }
    { file = "pool/main/g/gcc-9/libgcc-9-dev_9.2.1-9ubuntu2_arm64.deb"; sha256 = "4e4a83c4d83471eff726fd819b4e5d266fd01c0fbb66f1376718b78bfd4ae116"; }
    { file = "pool/main/libd/libdrm/libdrm-dev_2.4.99-1ubuntu1_arm64.deb"; sha256 = "69071f9360391bdf3ec9a3842aef851b05aac363e9c35b89a8c31f8227d31af5"; }
    { file = "pool/main/libd/libdrm/libdrm2_2.4.99-1ubuntu1_arm64.deb"; sha256 = "570407f42d8fac13c20be388fa45a1739ae0e900b30d9a924bd6599cd5e64d45"; }
    { file = "pool/main/m/mesa/libgbm-dev_19.2.1-1ubuntu1_arm64.deb"; sha256 = "7732f9ebe793b7ea5dd45b7c7c531446ea6d9a4f366229630e70a1bd9a58252a"; }
    { file = "pool/main/m/mesa/libgbm1_19.2.1-1ubuntu1_arm64.deb"; sha256 = "c982ba0bc2e00522e82bdde0b510fee50185b89670ff77853074d7b591c1f054"; }
    { file = "pool/main/m/mesa/libegl1-mesa-dev_19.2.1-1ubuntu1_arm64.deb"; sha256 = "d9641fc40648a334773c0f6bf4bb2ec0cc2597bfc519a8b5da3923e7a37d9f66"; }
    { file = "pool/main/m/mesa/libgles2-mesa-dev_19.2.1-1ubuntu1_arm64.deb"; sha256 = "8af36ab466d0512d479e996682a288e8e7345e2f55fac411c0b16bc629652781"; }
    { file = "pool/main/m/mesa/mesa-common-dev_19.2.1-1ubuntu1_arm64.deb"; sha256 = "a7fe8a288fd9ed96e24afe4dad794efdeafe42955981f9bb556017c37100d201"; }
    { file = "pool/main/libg/libglvnd/libglvnd-dev_1.1.1-0ubuntu1_arm64.deb"; sha256 = "fa1a7337d8b05eaf78e7a6faf92a4deb36f95d7c353fef5cec532589b4970315"; }
    { file = "pool/main/libg/libglvnd/libegl1_1.1.1-0ubuntu1_arm64.deb"; sha256 = "fbfebfb52f24e45f557954c3bfa67cf09c409dbf3cb524e561bece604700bb7a"; }
    { file = "pool/main/libg/libglvnd/libgles2_1.1.1-0ubuntu1_arm64.deb"; sha256 = "3555d264b1815c5b601f6ec849a582560c6511b278cfee02d3b1f518b5b6c208"; }
    { file = "pool/main/a/alsa-lib/libasound2-dev_1.1.9-0ubuntu1_arm64.deb"; sha256 = "a13b54fda5f644cea292e7f9ec4cd7199f3ec78e77f7879a1fad1bd08df60d35"; }
    { file = "pool/main/a/alsa-lib/libasound2_1.1.9-0ubuntu1_arm64.deb"; sha256 = "67938b676ef5b4e08126fb8451ef9285348bda985124e31e801a80cde77a1063"; }
    { file = "pool/main/s/systemd/libudev-dev_242-7ubuntu3_arm64.deb"; sha256 = "2785dd52d59b7a590f5471b474fd8f09fd2ef5c79d889510b740e5ce95fc69fa"; }
    { file = "pool/main/s/systemd/libudev1_242-7ubuntu3_arm64.deb"; sha256 = "af9cb069b6900a330d6ccf82f9e346ab1d74d5d8df76f4a50b76f63602904610"; }
  ];
  fetchDeb = d: pkgs.fetchurl {
    url = "http://old-releases.ubuntu.com/ubuntu/${d.file}";
    inherit (d) sha256;
  };

  sysroot = pkgs.stdenvNoCC.mkDerivation {
    pname = "arkos-sysroot";
    version = "eoan-arm64";
    dontUnpack = true;
    nativeBuildInputs = [ pkgs.dpkg ];
    installPhase = ''
      mkdir -p $out
      ${lib.concatMapStrings (d: "dpkg-deb -x ${fetchDeb d} $out\n") debs}
      # Absolute symlinks point into the device's root: into the sysroot.
      find $out -type l | while read -r link; do
        target=$(readlink "$link")
        case "$target" in
          /*) ln -sfn "$out$target" "$link" ;;
        esac
      done
      # GCC 16's drivers link -lgcc_s_asneeded, a script of its own: the
      # same here, for eoan's libgcc_s.
      echo 'INPUT ( AS_NEEDED ( -lgcc_s ) )' > $out/usr/lib/gcc/${multiarch}/9/libgcc_s_asneeded.so
    '';
    # aarch64 files: the host's fixups must keep off.
    dontFixup = true;
  };

  cross = pkgs.pkgsCross.aarch64-multiplatform.stdenv.cc;
  gcc = cross.cc;
  binutils = cross.bintools.bintools;
  tp = lib.removeSuffix "-" cross.targetPrefix; # aarch64-unknown-linux-gnu
  multiarch = "aarch64-linux-gnu";
  gccLib = "${gcc}/lib/gcc/${tp}/${gcc.version}";

  # Compiler drivers that see nothing but the sysroot (and GCC's own
  # freestanding headers and builtins).
  cflags = lib.concatStringsSep " " [
    "-nostdinc"
    "--sysroot=${sysroot}"
    "-B${binutils}/${tp}/bin"
    "-isystem ${gccLib}/include"
    "-isystem ${sysroot}/usr/include/${multiarch}"
    "-isystem ${sysroot}/usr/include"
    "-march=armv8-a -mtune=cortex-a35"
    # GCC 10+ calls helpers for atomics (__aarch64_ldadd4_rel ...) that
    # GCC 9's libgcc lacks; the Cortex-A35 (ARMv8.0, no LSE) would take the
    # plain instructions anyway.
    "-mno-outline-atomics"
  ];
  cxxflags = lib.concatStringsSep " " [
    "-nostdinc -nostdinc++"
    "--sysroot=${sysroot}"
    "-B${binutils}/${tp}/bin"
    "-isystem ${sysroot}/usr/include/c++/9"
    "-isystem ${sysroot}/usr/include/${multiarch}/c++/9"
    "-isystem ${sysroot}/usr/include/c++/9/backward"
    "-isystem ${gccLib}/include"
    "-isystem ${sysroot}/usr/include/${multiarch}"
    "-isystem ${sysroot}/usr/include"
    "-march=armv8-a -mtune=cortex-a35"
    # GCC 10+ calls helpers for atomics (__aarch64_ldadd4_rel ...) that
    # GCC 9's libgcc lacks; the Cortex-A35 (ARMv8.0, no LSE) would take the
    # plain instructions anyway.
    "-mno-outline-atomics"
  ];
  ldflags = lib.concatStringsSep " " [
    "-B${sysroot}/usr/lib/${multiarch}"
    "-B${gccLib}"
    "-L${sysroot}/usr/lib/gcc/${multiarch}/9"
    "-L${sysroot}/usr/lib/${multiarch}"
    "-L${sysroot}/lib/${multiarch}"
    "-Wl,-rpath-link,${sysroot}/usr/lib/${multiarch}:${sysroot}/lib/${multiarch}"
    "-Wl,--dynamic-linker=/lib/ld-linux-aarch64.so.1"
  ];
  # Linking C++: the sysroot's libstdc++, never GCC's own (-nostdlib++).
  driver = name: compiler: flags: linkFlags: libs: pkgs.writeShellScript name ''
    for a in "$@"; do
      case "$a" in
        -c|-S|-E|-M|-MM) exec ${gcc}/bin/${tp}-${compiler} ${flags} "$@" ;;
      esac
    done
    exec ${gcc}/bin/${tp}-${compiler} ${flags} ${linkFlags} "$@" ${libs}
  '';
  cc = driver "arkos-gcc" "gcc" cflags ldflags "-lm";
  cxx = driver "arkos-g++" "g++" cxxflags "-nostdlib++ ${ldflags}" "-lstdc++ -lm";

  toolchainFlags = [
    "-DCMAKE_SYSTEM_NAME=Linux"
    "-DCMAKE_SYSTEM_PROCESSOR=aarch64"
    "-DCMAKE_SYSROOT=${sysroot}"
    "-DCMAKE_C_COMPILER=${cc}"
    "-DCMAKE_CXX_COMPILER=${cxx}"
    "-DCMAKE_AR=${binutils}/bin/${tp}-ar"
    "-DCMAKE_RANLIB=${binutils}/bin/${tp}-ranlib"
    "-DCMAKE_STRIP=${binutils}/bin/${tp}-strip"
    "-DCMAKE_SKIP_RPATH=ON"
  ];

  # SDL3, built for the device against the sysroot and shipped inside the
  # port (ArkOS has only SDL2). Only what the handheld uses: KMS/DRM video
  # with EGL/GLES, ALSA, evdev/udev input. Those libraries (libdrm, libgbm,
  # libEGL, libGLESv2, libasound, libudev) are dlopen'ed from the device.
  sdl3 = pkgs.stdenv.mkDerivation {
    pname = "sdl3-r36s";
    inherit (pkgs.sdl3) version src;
    nativeBuildInputs = [ pkgs.cmake pkgs.pkg-config ];
    env = {
      PKG_CONFIG_LIBDIR = "${sysroot}/usr/lib/${multiarch}/pkgconfig:${sysroot}/usr/share/pkgconfig";
      PKG_CONFIG_SYSROOT_DIR = "${sysroot}";
    };
    cmakeFlags = toolchainFlags ++ [
      "-DCMAKE_BUILD_TYPE=Release"
      "-DSDL_SHARED=ON" "-DSDL_STATIC=OFF"
      "-DSDL_TESTS=OFF" "-DSDL_EXAMPLES=OFF" "-DSDL_INSTALL_TESTS=OFF"
      "-DSDL_X11=OFF" "-DSDL_WAYLAND=OFF" "-DSDL_VULKAN=OFF" "-DSDL_OPENGL=OFF"
      "-DSDL_PULSEAUDIO=OFF" "-DSDL_PIPEWIRE=OFF" "-DSDL_JACK=OFF" "-DSDL_SNDIO=OFF"
      "-DSDL_DBUS=OFF" "-DSDL_IBUS=OFF" "-DSDL_LIBURING=OFF" "-DSDL_CAMERA=OFF"
      # No X11/Wayland on purpose: the documented way past SDL's desktop check.
      "-DSDL_UNIX_CONSOLE_BUILD=ON"
      "-DSDL_KMSDRM=ON" "-DSDL_ALSA=ON" "-DSDL_LIBUDEV=ON" "-DSDL_OPENGLES=ON"
    ];
    dontPatchELF = true;
    dontStrip = true;
    dontFixup = true;
  };

  app = pkgs.stdenv.mkDerivation {
    pname = "globtoo-r36s-build";
    inherit version src;
    nativeBuildInputs = [ pkgs.cmake ];
    cmakeFlags = toolchainFlags ++ appFlags ++ [
      "-DSDL3_DIR=${sdl3}/lib/cmake/SDL3"
      "-DGLOBTOO_R36S=ON"
    ];
    dontFixup = true;
    postInstall = ''
      ${binutils}/bin/${tp}-strip "$out/bin/globtoo"
    '';
  };
in pkgs.runCommand "globtoo-${version}-r36s" {} ''
  mkdir -p "$out"
  cp -r ${app}/{bin,share} "$out/"
  chmod u+w "$out/bin"
  cp -a ${sdl3}/lib/libSDL3.so.0* "$out/bin/"
  install -m755 ${./r36s-launcher.sh} "$out/Globtoo.sh"
''
