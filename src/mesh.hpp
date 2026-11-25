// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "gles2.hpp"
#include <vector>
#include "math.hpp"

struct Mesh {
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLuint projected_vbo = 0;
    GLsizei index_count = 0;
    GLsizei projected_count = 0;
};

// UV sphere, radius 1, segments control detail
Mesh create_sphere(int slices, int stacks);
// Portion of the same geographic sphere, with local texture coordinates.
// Geometry and triangle anchors use GLOBAL UVs; sampling uses padded tile UVs.
Mesh create_sphere_region(int slices, int stacks, double u0, double v0,
                          double u1, double v1, double t0, double s0,
                          double t1, double s1);
void destroy_mesh(Mesh& m);
void draw_mesh(const Mesh& m, bool reprojected = false);
