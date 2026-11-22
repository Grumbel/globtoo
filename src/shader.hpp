// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <GLES2/gl2.h>
#include <string>

// Uniform locations are stable for the lifetime of a linked GLES program.
// Query them once instead of doing ~15 driver lookups per map draw/pass.
struct SceneUniformLocations {
    GLint view=-1, projection=-1, morph=-1, projection_from=-1,
          projection_to=-1, projection_blend=-1;
    GLint map_east=-1, map_north=-1, map_center=-1;
    GLint map_zoom=-1, map_distance=-1, wrap=-1, bounded_projection=-1,
          map_roll=-1, map_pan=-1;
};

struct Shader {
    GLuint program = 0;
    GLint  u_mvp = -1;
    GLint  u_model = -1;
    GLint  u_tex = -1;
    GLint  u_shading = -1;
    GLint  u_light_dir = -1;
    GLint  u_textured = -1, u_fill_opacity = -1, u_color = -1;
    GLint  u_viewport = -1;
    SceneUniformLocations scene{};
};

Shader load_shader(const char* vert_path, const char* frag_path);
void   destroy_shader(Shader& s);
void   use_shader(const Shader& s);
