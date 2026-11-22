#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Verify and install a prepared Moon/Mars/Io map in XDG user data.

Only accepts outputs from prepare_planetary_map.py (manifest, provenance,
checksummed tiles, previews and QA). No network access or runtime conversion.
Existing maps are preserved on verification/copy/publish failures.
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

# A small separate streaming digest helper keeps installation dependency-free:
# unlike source preparation, it does not need Pillow, NumPy or a GPU.
MAX_FILE_BYTES = 1024 * 1024 * 1024


def hash_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    total = 0
    with path.open('rb') as stream:
        while chunk := stream.read(1024 * 1024):
            total += len(chunk)
            if total > MAX_FILE_BYTES:
                raise ValueError(f'file exceeds {MAX_FILE_BYTES} bytes: {path}')
            digest.update(chunk)
    if not total:
        raise ValueError(f'file is empty: {path}')
    return digest.hexdigest(), total

BODIES = ('moon', 'mars', 'io')
HASH_RE = re.compile(r'[0-9a-f]{64}\Z')
TILE_RE = re.compile(r'tile-\d{3}-\d{3}\.jpg\Z')
FIXED_FILES = {'manifest.txt', 'preview.jpg', 'preview-small.jpg',
               'source.txt', 'alignment.png', 'alignment.json'}
MAX_TILES = 8192


def data_root(data_home: Path | None = None) -> Path:
    """Resolve an absolute XDG data root without creating directories."""
    if data_home is not None:
        root = Path(data_home)
        if not root.is_absolute():
            raise ValueError('--data-home must be an absolute directory')
        return root / 'globtoo'
    configured = os.environ.get('XDG_DATA_HOME', '')
    if configured and Path(configured).is_absolute():
        return Path(configured) / 'globtoo'
    home = os.environ.get('HOME', '')
    if not home or not Path(home).is_absolute():
        raise ValueError('set an absolute XDG_DATA_HOME or HOME')
    return Path(home) / '.local' / 'share' / 'globtoo'


def verify_prepared_map(source: Path, body: str | None = None) -> dict:
    """Check that a generated tile tree is internally intact and self-consistent.

    This does not independently verify a publisher's license, orientation, or
    the authenticity of the hash recorded by the operator; it detects damaged
    or incomplete outputs and prevents path traversal/symlink copying.
    """
    source = Path(source)
    if source.is_symlink() or not source.is_dir():
        raise ValueError('prepared map must be a real directory, not a symlink')
    record_file = source / 'source.json'
    if record_file.is_symlink() or not record_file.is_file():
        raise ValueError('missing source.json provenance record')
    record = json.loads(record_file.read_text(encoding='utf-8'))
    if not isinstance(record, dict) or record.get('schema') != 'globtoo-planetary-source-v1':
        raise ValueError('unsupported planetary source.json format')
    reported_body = record.get('body')
    if reported_body not in BODIES or (body is not None and reported_body != body):
        raise ValueError(f'prepared map body mismatch: {reported_body!r}')
    if not isinstance(record.get('source_url_or_citation'), str) or not record['source_url_or_citation'].strip():
        raise ValueError('missing source citation')
    if not isinstance(record.get('license_as_declared'), str) or not record['license_as_declared'].strip():
        raise ValueError('missing license statement')
    if record.get('output_coordinates') != 'planetocentric, east-positive, -180 degree left boundary':
        raise ValueError('map has unsupported coordinate convention')
    if not isinstance(record.get('source_sha256'), str) or not HASH_RE.fullmatch(record['source_sha256']):
        raise ValueError('missing or invalid source digest')

    hashes = record.get('files_sha256')
    if not isinstance(hashes, dict) or not hashes:
        raise ValueError('missing file hashes')
    names = set(hashes)
    if not FIXED_FILES.issubset(names) or not any(TILE_RE.fullmatch(name) for name in names):
        raise ValueError('incomplete prepared map payload')
    for name, expected in hashes.items():
        if not isinstance(name, str) or not (name in FIXED_FILES or TILE_RE.fullmatch(name)):
            raise ValueError(f'unexpected file in recorded map: {name!r}')
        if not isinstance(expected, str) or not HASH_RE.fullmatch(expected):
            raise ValueError(f'invalid digest for {name}')
    actual_names = {entry.name for entry in source.iterdir()}
    if actual_names != names | {'source.json'}:
        raise ValueError(f'unexpected or missing prepared map files: {sorted(actual_names ^ (names | {"source.json"}))}')
    for name, expected in hashes.items():
        path = source / name
        if path.is_symlink() or not path.is_file():
            raise ValueError(f'missing or non-regular file: {name}')
        if hash_file(path)[0] != expected:
            raise ValueError(f'SHA-256 mismatch in prepared map file: {name}')

    manifest_lines = (source / 'manifest.txt').read_text(encoding='ascii').splitlines()
    if len(manifest_lines) != 2 or manifest_lines[0] != 'GLOBTOO_RASTER_TILES 1':
        raise ValueError('invalid tile manifest header')
    try:
        width, height, size, gutter, columns, rows = map(int, manifest_lines[1].split())
    except ValueError as exc:
        raise ValueError('invalid tile manifest dimensions') from exc
    if not (width == 2 * height and width > 0 and
            width == record.get('source_width') and height == record.get('source_height') and
            size in (64, 128, 256, 512, 1024, 2048) and 2 <= gutter <= 16 and
            gutter * 2 < size and columns > 0 and rows > 0 and
            columns * rows <= MAX_TILES and
            columns == (width + (size - 2*gutter) - 1) // (size - 2*gutter) and
            rows == (height + (size - 2*gutter) - 1) // (size - 2*gutter)):
        raise ValueError('invalid tile manifest geometry')
    expected_tiles = {f'tile-{y:03d}-{x:03d}.jpg'
                      for y in range(rows) for x in range(columns)}
    if names - FIXED_FILES != expected_tiles:
        raise ValueError('tile files do not match manifest grid')
    return record


def install_map(source: Path, *, data_home: Path | None = None,
                body: str | None = None, replace: bool = False) -> Path:
    source = Path(source).absolute()
    record = verify_prepared_map(source, body)
    destination = data_root(data_home) / 'imagery' / record['body']
    if destination.is_symlink() or (destination.exists() and not destination.is_dir()):
        raise ValueError('installed map location is a file or symlink')
    if destination.exists() and not replace:
        raise ValueError(f'{record["body"]} map already installed; use --replace')
    if destination == source or source in destination.parents or destination in source.parents:
        raise ValueError('source cannot overlap the installation destination')
    destination.parent.mkdir(parents=True, exist_ok=True)
    staging = Path(tempfile.mkdtemp(prefix=f'.{destination.name}.staging-',
                                    dir=destination.parent))
    backup = None
    try:
        for entry in source.iterdir():
            shutil.copy2(entry, staging / entry.name, follow_symlinks=False)
        verify_prepared_map(staging, record['body'])
        if destination.exists():
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
        return destination
    finally:
        if staging.exists():
            shutil.rmtree(staging)
        # If rollback itself failed, keep the previous map's backup intact.


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('prepared_map', type=Path, help='output from prepare_planetary_map.py')
    parser.add_argument('--body', choices=BODIES, help='require a specific body')
    parser.add_argument('--replace', action='store_true', help='replace an installed map atomically')
    parser.add_argument('--verify', action='store_true', help='validate map without installing')
    parser.add_argument('--data-home', type=Path, help='absolute XDG data directory override')
    args = parser.parse_args(argv)
    try:
        if args.verify:
            record = verify_prepared_map(args.prepared_map, args.body)
            print(f'verified local {record["body"]} map: {args.prepared_map}')
        else:
            destination = install_map(args.prepared_map, body=args.body,
                                      data_home=args.data_home, replace=args.replace)
            print(f'Installed {destination.name} tiles: {destination}\n'
                  f'Launch: globtoo --body {destination.name}')
    except (OSError, ValueError, UnicodeError, json.JSONDecodeError) as exc:
        parser.exit(1, f'planetary map installation: {exc}\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
