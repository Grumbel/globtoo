// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "camera.hpp"
#include "shader.hpp"
#include "gles2.hpp"

struct Stars {
    GLuint vbo = 0;
    GLsizei count = 0;
    Shader shader{};
};

bool stars_init(Stars& s);
void stars_shutdown(Stars& s);
void stars_draw(const Stars& s, const Camera& cam, float aspect);
