// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <GLES2/gl2.h>
#include <string>

struct Shader {
    GLuint program = 0;
    GLint  u_mvp = -1;
    GLint  u_model = -1;
    GLint  u_tex = -1;
    GLint  u_shading = -1;
    GLint  u_light_dir = -1;
};

Shader load_shader(const char* vert_path, const char* frag_path);
void   destroy_shader(Shader& s);
void   use_shader(const Shader& s);
