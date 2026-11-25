#!/bin/bash
# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

if [ -d /opt/system/Tools/PortMaster ]; then
    controlfolder=/opt/system/Tools/PortMaster
elif [ -d /opt/tools/PortMaster ]; then
    controlfolder=/opt/tools/PortMaster
elif [ -d "${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster" ]; then
    controlfolder="${XDG_DATA_HOME:-$HOME/.local/share}/PortMaster"
else
    controlfolder=/roms/ports/PortMaster
fi
source "$controlfolder/control.txt" || exit 1
if [ -f "$controlfolder/mod_${CFW_NAME}.txt" ]; then
    source "$controlfolder/mod_${CFW_NAME}.txt"
fi
get_controls

GAMEDIR="/$directory/ports/globtoo"
cd "$GAMEDIR" || exit 1
exec > >(tee "$GAMEDIR/log.txt") 2>&1

export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig:-${SDL_GAMECONTROLLERCONFIG:-}}"
pm_platform_helper "$GAMEDIR/bin/globtoo"
./Globtoo.sh "$@"
status=$?
pm_finish
exit "$status"
