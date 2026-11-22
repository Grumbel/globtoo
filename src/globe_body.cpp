// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globe.hpp"
#include <cstdio>
#include <filesystem>

void globe_select_body(Globe& g, BodyId body) {
    if (g.body == body && g.texture.id != 0) return;
    destroy_texture(g.texture);
    g.body=body;
    g.surface=SurfaceLayer::Imagery; // Earth-only thematic rasters
    const std::string& path=g.body_textures[static_cast<std::size_t>(body)];
    if (!path.empty() && std::filesystem::is_regular_file(path)) {
        g.texture=load_texture(path.c_str());
    } else if (body!=BodyId::Earth && !g.imagery[static_cast<std::size_t>(body)].valid()) {
        std::fprintf(stderr,"%.*s: no surface map installed; showing untextured sphere.\n",
                     static_cast<int>(body_info(body).name.size()),
                     body_info(body).name.data());
    }
}

