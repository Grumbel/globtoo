<!--
SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
SPDX-License-Identifier: GPL-3.0-or-later
-->

# Command-line markers

Globtoo accepts an arbitrary number of independent markers, shown in 3D and in
all four flat projections. Markers are anchored to the WGS84 geographic surface,
so they follow globe rotation, map reprojection, pole crossings and unfolding.
They are screen-space pins with a fixed radius, not geodetic distances. Pins on
the hidden hemisphere are culled by the same projection logic used for country
labels. No external geocoding or network lookup is needed.

## Options

| Option | Meaning |
| --- | --- |
| `-m`, `--marker COORDS` | Add a marker (latitude, longitude by default). Repeatable. |
| `--marker-lonlat COORDS` | Add a marker using longitude-first numeric order. |
| `-l`, `--label TEXT` | Attach/replace a label for the most recently added marker. |
| `--marker-color #RRGGBB` | Set color for **subsequent** markers; default `#FFC54B`. |
| `--marker-size PIXELS` | Set radius for **subsequent** markers, range 2–32; default 7. |
| `--marker-file FILE` | Read CSV or `COORDS | label` rows, appending markers. |
| `--center COORDS` | Set initial viewing center; otherwise use the system timezone. |
| `--texture FILE` | Override the Earth texture; equivalent to the legacy positional filename. |
| `--help`, `-h` | Print usage without creating an SDL window. |
| `--version`, `-V` | Print the full version without creating an SDL window. |

For inline labels, use a pipe in the quoted `--marker` value:
`--marker '52.52,13.405|Berlin'`. The label option is useful when shells or
scripts generate coordinates separately from names.

## Accepted coordinate representations

| Representation | Example |
| --- | --- |
| Signed decimal degrees (latitude, longitude) | `52.5200,13.4050` |
| Decimal degrees, longitude first | `--marker-lonlat '13.4050,52.5200'` |
| Compass directions | `N51.5074, W0.1278`, `51.5074N 0.1278W` |
| Degrees/minutes/seconds | `51°30′26″N, 0°07′39″W` or `51:30:26N,0:07:39W` |
| Degrees and decimal minutes | `51°30.5'N,0°7.25'W` |
| Geo URI (RFC 5870) | `geo:48.8583,2.2945` or `geo:48.8583,2.2945,30;u=5` |
| ISO 6709 | `+52.52+013.405/`, `+5230+01324/`, `-334500+1511230/` |

Latitude must be between -90° and +90°, longitude between -180° and +180°.
Degrees/minutes/seconds must have minutes and seconds in the interval [0,60).
An invalid or ambiguous coordinate causes a CLI error and a nonzero exit code.
Directional letters automatically determine which coordinate is latitude or
longitude; otherwise the default is latitude first.

Geo URI altitude is accepted and ignored (the pin is on the surface). ISO 6709
compact DDMMSS / DDDMMSS notation is supported, including decimal seconds.
UTM/MGRS, plus codes, geocoded street addresses and non-WGS84 datums are not
supported. Longitude is positive east, latitude positive north. No geocoding
service is queried.

## Marker files

A UTF-8 CSV file may contain a header (`latitude,longitude,label`) and either
2 or 3 columns. A quoted third field may contain commas or doubled quote
characters. Empty labels are permitted. Lines beginning with `#` and blank
lines are ignored. A file may also contain one `COORDS | label` expression per
line, useful when its coordinates contain DMS punctuation.

```csv
latitude,longitude,label
52.5200,13.4050,Berlin
40.6892,-74.0445,"Liberty, Island"
48.8583,2.2945,Eiffel Tower
```

All file coordinates use latitude-first ordering, except when directional
letters make the order explicit. `--marker-file` may be repeated; all rows are
appended. File errors report the path and line number. Markers are drawn in
argument order; for dense overlays, later markers can obscure earlier pins.

## Version numbers

The single authoritative base version is top-level `VERSION`. Development
builds use `{base}-dev.{revCount}+g{shortRev}` with an optional `-dirty` suffix
for locally modified trees. A missing Git revision count falls back to zero;
Nix builds use the flake's revision metadata and pass the resulting full string
into CMake. A release `VERSION` without `-dev` is shown exactly as written,
without a Git suffix. `--version` and the window title use the same string.

## On Moon, Mars and Io

`--marker`, `--marker-file`, and `--center` are interpreted in Globtoo's
**east-positive, north-positive** spherical viewer coordinates for the
selected body, not as WGS84 Earth locations. Source planetary gazetteers
may specify west-positive longitudes or planetographic latitudes and require
normalization. For nine sourced reference landmarks, see
[planetary maps](PLANETARY_MAPS.md#named-planetary-features) and
`globtoo --list-features`; `--focus NAME` centers on one of them.
