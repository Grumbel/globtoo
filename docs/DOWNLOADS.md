<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Package variants and on-demand images

The Nix flake exposes two Linux packages built from the **same C++ viewer**:

| Nix package | Offline imagery | Countries / landmarks | Network / crypto libraries |
| --- | --- | --- | --- |
| `.#default` | Pinned 4K Earth map | 50m and 10m boundaries, 10m cities/rivers | No downloader, no libcurl/OpenSSL requirement |
| `.#globtoo-lite` | Solid-color fallback until downloaded | 50m country boundaries; 10m additions omitted | libcurl + OpenSSL, for an **explicit** fetch only |

The lite package starts without any network access, even when the image is
absent. The globe, projections, and available country overlays remain usable.
No map is downloaded when switching bodies, scrolling, zooming, or starting.
The full package uses the installed imagery even if there is a cached copy.
Other Earth raster layers and planetary mosaics remain optional, separately
supplied sources, not automatic downloads.

## Installing the smaller package

```sh
nix run .#globtoo-lite
# Or install it and run "globtoo" directly.
```

On a curl-enabled build, the user may opt in to fetching the base Earth image:

```sh
globtoo --download-asset earth
# or: nix run .#globtoo-lite -- --download-asset earth
```

This command downloads a single asset and exits **before initializing SDL**.
The source is the same 4K Earth image and SHA-256 pin specified in `flake.nix`.
Only the name `earth` is available until other rights-cleared maps have their
metadata, bounds, orientation, and actual bytes verified. Command line users
cannot change the URL or digest. `--texture FILE` is still supported for
arbitrary **local** imagery, subject to the normal coordinate restrictions.

With `$XDG_CACHE_HOME` set to an absolute path, the downloaded file is stored
at `$XDG_CACHE_HOME/globtoo/assets/earth.jpg`. Otherwise it uses
`$HOME/.cache/globtoo/assets/earth.jpg`. An unavailable home/cache path gives
an error rather than silently writing in the current working directory.
`GLOBE_ASSET_DIR` and explicit CLI texture paths continue to override the
installed and cached resources.

## Verification and failure handling

- The HTTPS fetcher enforces TLS certificate verification and permits only
  HTTPS redirects. The build's checksum catalog is compiled in; it cannot be
  changed by a server-side manifest or a URL argument.
- The Earth download is limited to 12 MiB, including transfers with no
  `Content-Length` header. A short progress indication is printed on stderr.
- The SHA-256 digest is checked against the pinned checksum **before**
  publishing the file. A completed download is atomically moved into the cache.
  Cache hits are rehashed and do not trigger a network request.
- Failed, oversized, or mismatched transfers remove their temporary file and
  leave an existing cache file untouched. SIGINT/SIGTERM during a live transfer
  request cancellation and trigger the same cleanup. A forced crash/SIGKILL
  can leave an ignored `.tmp.*` file, but never a published incomplete map.
  Retry the same command to start a clean transfer; HTTP range resume is not
  yet implemented. A failed fetch cannot alter installed package assets.
- The normal viewer **does not perform a network request**, including when
  cached data is missing or corrupt. The downloaded file is treated like a
  local user-provided texture at display time. Download verification prevents
  an invalid *new* cache entry from being published; it is not a general file
  integrity monitor against modifications to the cache after installation.

A standard locally built viewer has no downloader unless explicitly configured
with `-DGLOBTOO_ENABLE_DOWNLOADS=ON` when invoking CMake, plus libcurl and
OpenSSL development libraries. Both enabled and disabled configurations are
covered by headless C++ regression tests; no live network is required.

## Sources and provenance

The initial catalog points to the NASA Blue Marble-derived Earth daytime map
used by `turban/webgl-earth` and pinned in this project's `flake.nix` as:

- URL: `https://raw.githubusercontent.com/turban/webgl-earth/master/images/2_no_clouds_4k.jpg`
- SHA-256: `61cd3d27489d80ed025d7954539a6d90b50d4433bbdd27e8c1af8a6f15f71e86`
- Maximum accepted download size: 12 MiB

This is **not** a newly fetched/rehash-verified upstream object; the checksum
is carried over from the existing Nix pin. Independent online verification,
any higher-resolution NASA tiles, and actual planetary products remain future
work (see `PLANETARY_MAPS.md` and `TODO.md`).
