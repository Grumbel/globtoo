// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "map_view.hpp"
#include "raster_cache.hpp"
#include "bodies.hpp"
#include <array>
#include <string>

enum class SurfaceLayer { Imagery, Relief, Elevation, Population };

struct Globe {
    Mesh    mesh;
    Shader  shader;
    Texture texture;
    RasterCache imagery;
    RasterCache relief;
    RasterCache elevation;
    RasterCache population;
    BodyId body = BodyId::Earth;
    // Earth is shipped with a default map; planetary textures are supplied
    // explicitly via CLI until reviewed source assets have been packaged.
    std::array<std::string,body_catalog.size()> body_textures{};
    SurfaceLayer surface = SurfaceLayer::Imagery;
    bool    shading = true;
    bool    texture_visible = true;  // Independent of political country colors
};

bool globe_init(Globe& g, const char* texture_path,
                const char* relief_manifest_path = nullptr,
                const char* elevation_manifest_path = nullptr,
                const char* population_manifest_path = nullptr,
                const char* imagery_manifest_path = nullptr);
void globe_shutdown(Globe& g);
// Changes the selected body's imagery, retaining globe orientation/zoom.
// A body without a supplied map intentionally uses a neutral surface color.
void globe_select_body(Globe& g, BodyId body);
// When texture is disabled, render the solid ocean base instead.
void globe_draw(Globe& g, const Camera& cam, float aspect,
                const MapView& map);
