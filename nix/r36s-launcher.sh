#!/bin/sh
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu
cd -- "$(dirname -- "$0")"
export SDL_VIDEO_DRIVER="${SDL_VIDEO_DRIVER:-kmsdrm}"
export LD_LIBRARY_PATH="$PWD/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec ./bin/globtoo "$@"
