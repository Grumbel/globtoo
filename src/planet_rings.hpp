// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bodies.hpp"
#include "camera.hpp"
#include "map_view.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "gles2.hpp"

// A flat ring annulus in the body's equatorial plane (y = 0), drawn after
// the globe. Its radial structure is a procedural RingProfile texture.
struct PlanetRings {
    BodyId body = BodyId::Saturn;
    float inner = 0, outer = 0; // in globe radii (body mean radius = 1)
    Texture texture{};
    Shader shader{};
    GLuint vbo = 0;
    GLsizei vertices = 0;
    GLint u_eye = -1, u_ring_range = -1, u_fade = -1;
    bool valid() const { return vbo != 0 && texture.id != 0 && shader.program != 0; }
};

bool planet_rings_init(PlanetRings& rings, BodyId body);
void planet_rings_shutdown(PlanetRings& rings);
// Outer ring radius in globe radii when `body` is the ringed body, else 1.
float planet_rings_content_radius(const PlanetRings& rings, BodyId body);
// Draws only for the ringed body; fades out as the globe morphs to a map.
void planet_rings_draw(const PlanetRings& rings, BodyId body, const Camera& camera,
                       float aspect, const MapView& map);
