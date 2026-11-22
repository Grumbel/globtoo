# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile Natural Earth 10m populated places and rivers into compact GLBFEAT1.

No Python dependencies beyond the standard library. Inputs must be valid GeoJSON;
Nix pins the upstream sources to a commit and content hash.
"""
import argparse
import json
import math
import pathlib
import struct

MAGIC = b"GLBFEAT1"
VERSION = 1


def valid_point(point):
    return (isinstance(point, (list, tuple)) and len(point) >= 2
            and all(isinstance(x, (int, float)) and math.isfinite(x) for x in point[:2])
            and -180 <= point[0] <= 180 and -90 <= point[1] <= 90)


def as_int(value, default=0):
    try:
        n = int(value)
        return n if n >= 0 else default
    except (ValueError, TypeError, OverflowError):
        return default


def features(path):
    with open(path, encoding="utf-8") as stream:
        data = json.load(stream)
    if data.get("type") != "FeatureCollection" or not isinstance(data.get("features"), list):
        raise ValueError(f"not a GeoJSON FeatureCollection: {path}")
    return data["features"]


def compile_places(path):
    result = []
    for feature in features(path):
        geometry = feature.get("geometry") or {}
        if geometry.get("type") != "Point":
            continue
        point = geometry.get("coordinates")
        if not valid_point(point):
            continue
        props = {k.upper(): v for k, v in (feature.get("properties") or {}).items()}
        name = props.get("NAME_EN") or props.get("NAMEASCII") or props.get("NAME")
        if not isinstance(name, str) or not name.strip():
            continue
        rank = min(255, as_int(props.get("SCALERANK"), 10))
        pop = min(0xffffffff, as_int(props.get("POP_MAX")))
        capital = as_int(props.get("ADM0CAP")) > 0 or props.get("FEATURECLA") in (
            "Admin-0 capital", "Admin-0 capital alt")
        alternate = props.get("NAMEALT") or props.get("NAME") or ""
        if not isinstance(alternate, str) or alternate == name:
            alternate = ""
        label = name.strip().encode("utf-8")[:65535]
        alt = alternate.strip().encode("utf-8")[:65535]
        result.append((float(point[0]), float(point[1]), pop, rank, int(capital), label, alt))
    # Ranked source order doesn't determine priority: a country's capital
    # always survives low zoom, then other cities ordered by prominence.
    result.sort(key=lambda p: (-p[4], p[3], -p[2], p[5]))
    return result


def normalize(v):
    n = math.sqrt(sum(c * c for c in v))
    return [c / n for c in v]


def xyz(point):
    lon, lat = map(math.radians, point[:2])
    return [math.cos(lat) * math.cos(lon), math.sin(lat), -math.cos(lat) * math.sin(lon)]


def segment_steps(a, b):
    # Major river arcs must follow the sphere, not form straight chords. Keep
    # vertex count bounded even on malformed/very sparse input.
    dot = max(-1., min(1., sum(x*y for x, y in zip(a, b))))
    return min(720, max(1, math.ceil(math.degrees(math.acos(dot)) / .5)))


def compile_rivers(path):
    result = []
    for feature in features(path):
        geo = feature.get("geometry") or {}
        ty = geo.get("type")
        if ty not in ("LineString", "MultiLineString"):
            continue
        props = {k.upper(): v for k, v in (feature.get("properties") or {}).items()}
        rank = min(255, as_int(props.get("SCALERANK"), 10))
        if rank > 10:
            continue
        lines = [geo.get("coordinates") or []] if ty == "LineString" else geo.get("coordinates") or []
        for line in lines:
            for p0, p1 in zip(line, line[1:]):
                if not valid_point(p0) or not valid_point(p1):
                    continue
                a, b = xyz(p0), xyz(p1)
                count = segment_steps(a, b)
                if sum(x*y for x,y in zip(a,b)) < -.999:  # ambiguous antipodal line
                    continue
                last = a
                for i in range(1, count + 1):
                    t = i / count
                    p = normalize([(1-t)*x+t*y for x, y in zip(a, b)])
                    result.append((last, p, rank))
                    last = p
    return result


def write_binary(out, places, rivers):
    with open(out, "wb") as stream:
        stream.write(MAGIC)
        stream.write(struct.pack("<III", VERSION, len(places), len(rivers)))
        for lon, lat, pop, rank, capital, name, alt in places:
            stream.write(struct.pack("<ffIBBHH", lon, lat, pop, rank, capital,
                                     len(name), len(alt)))
            stream.write(name)
            stream.write(alt)
        for a, b, rank in rivers:
            stream.write(struct.pack("<ffffffB", *a, *b, rank))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--places", type=pathlib.Path, required=True)
    parser.add_argument("--rivers", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    places = compile_places(args.places)
    rivers = compile_rivers(args.rivers)
    if not places or not rivers:
        parser.error("source data produced no populated places or rivers")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    write_binary(args.output, places, rivers)
    print(f"geographic features: {len(places)} places, {len(rivers)} river segments -> {args.output}")


if __name__ == "__main__":
    main()
