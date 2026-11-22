#pragma once

#include <string>
#include <string_view>

// SDL_GetBasePath() is supplied by the application at startup. Keeping the
// lookup itself SDL-free makes asset discovery usable from tests/tools.
void resources_set_executable_dir(std::string directory);

// Find a file installed under share/globtoo, next to a development binary, or
// in the source working directory. GLOBE_ASSET_DIR overrides every default.
std::string resource_path(std::string_view relative_path);
