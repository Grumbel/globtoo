#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build a self-contained, auditable Globtoo planetary map from a local mosaic.

No network access and no source metadata inference.  The operator supplies
original projection/axis/licensing facts, ideally from the publisher's metadata.
A supplied SHA-256 *pin* is checked before any image is decoded.  Without a
pin the digest is recorded as local evidence only, NOT as upstream verification.

Output is a standard --body-imagery-dir tree (manifest, tiles, previews) plus
source.json, source.txt, and a landmark registration report.  A staging tree
is built before a destination can be replaced, protecting an existing map from
failed conversions.  The original source image is not copied or modified.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

from PIL import Image

if __package__:
    from .build_raster_tiles import LARGE_PIXEL_LIMIT, build, source_size
    from .planetary_alignment import render
    from .planetary_bodies import BODIES
else:
    from build_raster_tiles import LARGE_PIXEL_LIMIT, build, source_size
    from planetary_alignment import render
    from planetary_bodies import BODIES

# Large enough for the 6 GB uncompressed LROC WAC 100 m Moon BigTIFF.
MAX_SOURCE_BYTES = 8 * 1024 * 1024 * 1024
SHA256_PATTERN = re.compile(r'^[0-9a-fA-F]{64}$')


def hash_file(path: Path, limit: int = MAX_SOURCE_BYTES) -> tuple[str, int]:
    """Hash in chunks with a strict byte limit (also handles growing files)."""
    digest = hashlib.sha256()
    size = 0
    with path.open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            size += len(chunk)
            if size > limit:
                raise ValueError(f'source exceeds {limit} byte limit: {path}')
            digest.update(chunk)
    if not size:
        raise ValueError('source file is empty')
    return digest.hexdigest(), size


def prepare(source: Path, destination: Path, *, body: str, source_url: str,
            license_text: str, positive: str, left: int, latitude: str,
            projection: str, expected_sha256: str | None = None,
            tile_size: int = 1024, gutter: int = 4, qa_width: int = 2048,
            replace: bool = False, top: str = 'north',
            preprocessing: str | None = None) -> dict:
    source = Path(source).absolute()
    destination = Path(destination).absolute()
    if body not in BODIES:
        raise ValueError(f'unsupported body: {body}')
    if projection != 'plate-carree':
        raise ValueError('only global plate-carree imagery is supported')
    if positive not in ('east', 'west') or left not in (0, 180):
        raise ValueError('explicit east|west longitude and left boundary 0|180 are required')
    if top not in ('north', 'south'):
        raise ValueError('explicit north|south top edge is required')
    if latitude not in ('planetocentric', 'planetographic'):
        raise ValueError('explicit planetocentric|planetographic latitude is required')
    if not source_url.strip() or not license_text.strip():
        raise ValueError('a source publication URL/citation and license statement are required')
    if preprocessing is not None and (not preprocessing.strip() or '\n' in preprocessing):
        raise ValueError('preprocessing note must be a non-empty single line')
    if expected_sha256 is not None and not SHA256_PATTERN.fullmatch(expected_sha256):
        raise ValueError('expected SHA-256 must contain exactly 64 hexadecimal characters')
    if not source.is_file() or source.is_symlink():
        raise ValueError('source must be an existing regular image (not a symlink)')
    if destination.is_symlink() or (destination.exists() and not destination.is_dir()):
        raise ValueError('output must be a directory, not a file or symlink')
    if source == destination or destination in source.parents:
        raise ValueError('source cannot be inside output directory')
    if destination.exists() and not replace:
        raise ValueError('output already exists; use --replace to replace it')
    if qa_width < 2 or qa_width > 8192 or qa_width % 2:
        raise ValueError('QA width must be even, between 2 and 8192')

    # Check the pinned payload before PIL opens it.  An unpinned local digest
    # helps provenance but must never be presented as publisher verification.
    digest, size = hash_file(source)
    if expected_sha256 and digest != expected_sha256.lower():
        raise ValueError(f'source SHA-256 mismatch: got {digest}, expected {expected_sha256.lower()}')
    # Header-only probe; sources above 250 megapixels are tiled by libvips.
    width, height = source_size(source)
    if width != 2 * height or width * height > LARGE_PIXEL_LIMIT or height < 1:
        raise ValueError('source must be a full-world 2:1 raster, at most 8 gigapixels')

    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f'.{destination.name}.staging-',
                                    dir=destination.parent))
    backup = None
    try:
        build(source, staging, tile_size, gutter, positive, left, latitude, body, top)
        # QA uses the normalized preview, so the converter and annotation share
        # a coordinate frame, without repeating the source transformation.
        render(staging, staging / 'alignment.png', body=body, normalized=True,
               width=qa_width, report=staging / 'alignment.json')
        # The report must refer to the *published* directory, not the
        # randomly named temporary staging directory used during preparation.
        report_path = staging / 'alignment.json'
        alignment = json.loads(report_path.read_text(encoding='utf-8'))
        alignment['input'] = str(destination)
        report_path.write_text(json.dumps(alignment, indent=2) + '\n', encoding='utf-8')
        if hash_file(source) != (digest, size):
            raise ValueError('source changed while map tiles were being prepared')
        (staging / 'source.txt').write_text(
            f'Body: {body}\nSource: {source_url}\nLicense (operator-declared): {license_text}\n'
            f'Original filename: {source.name}\nSource SHA-256: {digest}\n'
            f'Pin checked: {"yes" if expected_sha256 else "no (local hash only)"}\n'
            f'Projection: plate-carree, {top}-up (operator-declared)\n'
            f'Longitude: {positive}-positive, left={left} degrees (operator-declared)\n'
            f'Latitude: {latitude} (operator-declared)\n'
            + (f'Preprocessing: {preprocessing}\n' if preprocessing else '') +
            'Output: east-positive, planetocentric, -180 degrees at left\n'
            'Visual Gazetteer overlay is for QA; it does not certify registration.\n',
            encoding='utf-8')
        output_hashes = {}
        for path in sorted(staging.iterdir()):
            if path.is_file():
                output_hashes[path.name] = hash_file(path)[0]
        record = {
            'schema': 'globtoo-planetary-source-v1',
            'body': body,
            'source_filename': source.name,
            'source_url_or_citation': source_url,
            'license_as_declared': license_text,
            'source_bytes': size,
            'source_sha256': digest,
            'source_sha256_verified_against_supplied_pin': expected_sha256 is not None,
            'source_width': width,
            'source_height': height,
            'source_projection_as_declared': projection,
            'source_north_up_as_declared': top == 'north',
            'source_longitude_positive_as_declared': positive,
            'source_left_longitude_as_declared': left,
            'source_latitude_type_as_declared': latitude,
            'output_coordinates': 'planetocentric, east-positive, -180 degree left boundary',
            'output_tile_size': tile_size,
            'output_tile_gutter': gutter,
            'files_sha256': output_hashes,
            'source_preprocessing': preprocessing,
            'limitations': ('Publisher metadata, license, datum and feature alignment are '
                            'operator assertions; SHA-256 pin matching proves payload '
                            'identity only, not scientific map registration.'),
        }
        (staging / 'source.json').write_text(json.dumps(record, indent=2, sort_keys=True)
                                             + '\n', encoding='utf-8')

        if destination.exists():
            # Rename the complete previous tree out of the way.  If the new
            # publication fails, restore it.  No in-place tile replacement.
            backup = Path(tempfile.mkdtemp(prefix=f'.{destination.name}.backup-',
                                            dir=destination.parent))
            backup.rmdir()
            os.replace(destination, backup)
        try:
            os.replace(staging, destination)
        except OSError:
            if backup is not None:
                os.replace(backup, destination)
                backup = None
            raise
        if backup is not None:
            shutil.rmtree(backup)
            backup = None
        return record
    finally:
        if staging.exists():
            shutil.rmtree(staging)
        # A backup is retained if rollback itself failed (never delete data).


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path, help='locally acquired original global mosaic')
    parser.add_argument('output', type=Path, help='new tile output directory')
    parser.add_argument('--body', choices=BODIES, required=True)
    parser.add_argument('--source-url', required=True, help='original publication/URL or citation')
    parser.add_argument('--license', required=True, help='operator-confirmed redistribution license')
    parser.add_argument('--projection', choices=('plate-carree',), required=True)
    parser.add_argument('--positive', choices=('east', 'west'), required=True)
    parser.add_argument('--left', choices=(0, 180), type=int, required=True)
    parser.add_argument('--latitude', choices=('planetocentric', 'planetographic'), required=True)
    parser.add_argument('--top', choices=('north', 'south'), default='north',
                        help='pole at the source top edge (some Venus mosaics are south-up)')
    parser.add_argument('--expected-sha256', help='pin obtained independently, checked before decoding')
    parser.add_argument('--tile-size', type=int, default=1024)
    parser.add_argument('--gutter', type=int, default=4)
    parser.add_argument('--qa-width', type=int, default=2048)
    parser.add_argument('--replace', action='store_true', help='replace previous completed output')
    parser.add_argument('--preprocessing',
                        help='one-line record of geometric preprocessing already applied to SOURCE')
    args = parser.parse_args(argv)
    try:
        record = prepare(args.source, args.output, body=args.body,
                         source_url=args.source_url, license_text=args.license,
                         projection=args.projection, positive=args.positive,
                         left=args.left, latitude=args.latitude,
                         expected_sha256=args.expected_sha256,
                         tile_size=args.tile_size, gutter=args.gutter,
                         qa_width=args.qa_width, replace=args.replace, top=args.top,
                         preprocessing=args.preprocessing)
    except (OSError, ValueError, Image.DecompressionBombError) as exc:
        parser.exit(1, f'planetary map preparation: {exc}\n')
    print(f'{args.body}: prepared {args.output}\n'
          f'Source SHA-256: {record["source_sha256"]}\n'
          f'Upstream pin checked: {"yes" if args.expected_sha256 else "no"}\n'
          f'Open {args.output / "alignment.png"} to inspect landmark positions.\n'
          f'Launch: globtoo --body {args.body} --body-imagery-dir {args.body}={args.output}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
