#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Orientation and failure-mode tests for planetary map normalization."""

from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "tools" / "normalize_planet_texture.py"


def run(*argv: str, success: bool = True) -> None:
    result = subprocess.run([sys.executable, str(TOOL), *map(str, argv)],
                            text=True, capture_output=True)
    assert (result.returncode == 0) == success, result.stderr


def test() -> None:
    with TemporaryDirectory(prefix="globtoo-planet-") as folder:
        source = Path(folder) / "source.png"
        target = Path(folder) / "target.png"
        src = Image.new("RGB", (8, 4))
        for y in range(4):
            for x in range(8):
                src.putpixel((x, y), (x * 20, y * 50, 10))
        src.save(source)

        for positive in ("east", "west"):
            for left in (0, 180):
                run(source, target, "--positive", positive, "--left", str(left))
                with Image.open(target) as dst:
                    assert dst.size == (8, 4)
                    for y in range(4):
                        for x in range(8):
                            # Exact longitude mapping at pixel CENTERS.
                            # left = 0: half-turn; west: reverse sequence.
                            old_x = (7-x if positive == "west" else x)
                            if left == 0:
                                old_x = (old_x + 4) % 8
                            assert dst.getpixel((x,y)) == src.getpixel((old_x,y)), \
                                (positive,left,x,y,old_x)

        run(source, target, "--positive", "east", "--left", "180",
            "--max-width", "4")
        with Image.open(target) as img:
            assert img.size == (4, 2)

        broken = Path(folder) / "not-equirectangular.png"
        Image.new("RGB", (8, 5)).save(broken)
        run(broken, target, "--positive", "east", "--left", "180", success=False)
        run(source, source, "--positive", "east", "--left", "180", success=False)
        run(source, target, "--positive", "east", "--left", "180",
            "--max-width", "3", success=False)
        assert not Path(folder, "missing.jpg").exists()
        run(Path(folder, "missing.jpg"), target,
            "--positive", "east", "--left", "180", success=False)
    print("planet texture orientation tests passed")


if __name__ == "__main__":
    test()
