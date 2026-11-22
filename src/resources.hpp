// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

// SDL_GetBasePath() is supplied by the application at startup. Keeping the
// lookup itself SDL-free makes asset discovery usable from tests/tools.
void resources_set_executable_dir(std::string directory);

// Find a file installed under share/globtoo, next to a development binary, or
// in the source working directory. GLOBE_ASSET_DIR overrides every default.
std::string resource_path(std::string_view relative_path);

// Per-user installation location for vetted planetary imagery. Only known
// Moon/Mars/Io texture and tile-manifest resources are accepted; files here
// override a system package, but not an explicit GLOBE_ASSET_DIR/CLI choice.
std::string resource_user_data_path(std::string_view relative_path);

// Optional XDG cache location for a downloaded resource. Returns an empty
// path when no suitable absolute cache root can be resolved. Does not create
// directories or perform any I/O beyond environment lookup.
std::string resource_cache_path(std::string_view relative_path);
