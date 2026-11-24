# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared planetary body catalog for Globtoo's offline map tools.

Reads data/bodies.tsv (which mirrors src/bodies.hpp and is checked against it
by the C++ test suite). BODIES lists every non-Earth body that can receive a
planetary surface map, in the viewer's selection order.
"""
from pathlib import Path

CATALOG = Path(__file__).resolve().parents[1] / 'data' / 'bodies.tsv'
KINDS = ('planet', 'dwarf-planet', 'moon')
SURFACES = ('solid', 'clouds')


def read_catalog(filename: Path = CATALOG) -> dict[str, dict]:
    bodies: dict[str, dict] = {}
    with Path(filename).open(encoding='utf-8') as stream:
        for line in stream:
            if not line.strip() or line.startswith('#'):
                continue
            fields = line.rstrip('\n').split('\t')
            if len(fields) != 6:
                raise ValueError(f'body record needs 6 tab-separated fields: {line!r}')
            slug, name, kind, parent, radius, surface = fields
            if (not slug.isascii() or not slug.isalpha() or not slug.islower() or
                    kind not in KINDS or surface not in SURFACES or slug in bodies):
                raise ValueError(f'invalid body record: {line!r}')
            if (kind == 'moon') == (parent == '-'):
                raise ValueError(f'only moons have a parent body: {slug}')
            bodies[slug] = dict(slug=slug, name=name, kind=kind,
                                parent=None if parent == '-' else parent,
                                mean_radius_km=float(radius), surface=surface)
    for body in bodies.values():
        if body['parent'] is not None and body['parent'] not in bodies:
            raise ValueError(f'unknown parent body: {body["parent"]}')
    if 'earth' not in bodies:
        raise ValueError('body catalog must contain earth')
    return bodies


BODY_INFO = read_catalog()
BODIES = tuple(slug for slug in BODY_INFO if slug != 'earth')


def require_body(body: str) -> str:
    if body not in BODIES:
        raise ValueError(f'unsupported body: {body} (expected one of: {", ".join(BODIES)})')
    return body
