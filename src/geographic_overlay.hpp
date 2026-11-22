// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "camera.hpp"
#include "geographic_features.hpp"
#include "map_view.hpp"
#include "shader.hpp"
#include <GLES2/gl2.h>
#include <array>

struct GeographicOverlay {
    GeographicFeatures features;
    Shader river_shader{};
    std::array<GLuint,3> river_vbo{};
    std::array<GLsizei,3> river_vertices{};
};

// Missing optional data is not fatal (e.g. developer builds without GeoJSON).
bool geographic_overlay_init(GeographicOverlay& overlay);
void geographic_overlay_shutdown(GeographicOverlay& overlay);
void geographic_overlay_draw_rivers(const GeographicOverlay& overlay,
                                    const Camera& camera, float aspect,
                                    const MapView& map, bool visible);
