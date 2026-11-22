// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <GLES2/gl2.h>

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

// Load RGB/RGBA JPEG or PNG via stb_image (header-only)
Texture load_texture(const char* path, bool repeat = true);
void    destroy_texture(Texture& t);
void    bind_texture(const Texture& t, int unit = 0);
