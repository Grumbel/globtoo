# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

{ pkgs, version, r36s }:

let
  portmaster = pkgs.runCommand "globtoo-${version}-r36s-portmaster" {
    dontFixup = true;
  } ''
    mkdir -p "$out"
    cp -r ${r36s} "$out/globtoo"
    chmod u+w "$out/globtoo"
    install -m755 ${./portmaster/Globtoo.sh} "$out/Globtoo.sh"
    cp ${../data/icons/globtoo-256.png} "$out/globtoo/cover.png"
    cp ${../docs/screenshots/globtoo.png} "$out/screenshot.png"
    cp ${./portmaster/README.md} "$out/README.md"
    substitute ${./portmaster/port.json} "$out/port.json" \
      --subst-var-by version ${pkgs.lib.escapeShellArg version}
  '';
  portmasterZip = pkgs.runCommand "globtoo-${version}-r36s-portmaster-zip" {
    nativeBuildInputs = [ pkgs.zip ];
  } ''
    mkdir -p "$out"
    cp -r ${portmaster} port
    chmod -R u+w port
    cd port
    zip -qr9 "$out/globtoo.zip" .
  '';
in {
  globtoo-r36s-portmaster = portmaster;
  globtoo-r36s-portmaster-zip = portmasterZip;
}
