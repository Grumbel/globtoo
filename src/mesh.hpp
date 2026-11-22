// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <GLES2/gl2.h>
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
void destroy_mesh(Mesh& m);
void draw_mesh(const Mesh& m, bool reprojected = false);
