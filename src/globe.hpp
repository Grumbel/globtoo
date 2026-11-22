// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "map_view.hpp"
#include "raster_cache.hpp"

enum class SurfaceLayer { Imagery, Relief, Elevation, Population };

struct Globe {
    Mesh    mesh;
    Shader  shader;
    Texture texture;
    RasterCache relief;
    RasterCache elevation;
    RasterCache population;
    SurfaceLayer surface = SurfaceLayer::Imagery;
    bool    shading = true;
    bool    texture_visible = true;  // Independent of political country colors
};

bool globe_init(Globe& g, const char* texture_path,
                const char* relief_manifest_path = nullptr,
                const char* elevation_manifest_path = nullptr,
                const char* population_manifest_path = nullptr);
void globe_shutdown(Globe& g);
// When texture is disabled, render the solid ocean base instead.
void globe_draw(Globe& g, const Camera& cam, float aspect,
                const MapView& map);
