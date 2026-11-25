// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "camera.hpp"
#include "geographic_features.hpp"
#include "map_view.hpp"
#include "shader.hpp"
#include "gles2.hpp"
#include <array>

struct GeographicOverlay {
    GeographicFeatures features;
    Shader river_shader{};
    // Per detail tier: ribbon quads (4 vertices each), drawn in chunks that
    // share one GLES2 16-bit index buffer.
    std::array<GLuint,3> river_vbo{};
    std::array<GLsizei,3> river_quads{};
    GLuint river_ibo=0;
};

// Typical full channel width, in km, drawn for a Natural Earth river rank.
float river_width_km(unsigned scale_rank);

// Missing optional data is not fatal (e.g. developer builds without GeoJSON).
bool geographic_overlay_init(GeographicOverlay& overlay);
void geographic_overlay_shutdown(GeographicOverlay& overlay);
void geographic_overlay_draw_rivers(const GeographicOverlay& overlay,
                                    const Camera& camera, float aspect,
                                    const MapView& map, bool visible);
