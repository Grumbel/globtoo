// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <GLES2/gl2.h>
#include <vector>

struct Texture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

// CPU-side RGBA8 pixels, top row first.
struct ImageRGBA {
    std::vector<unsigned char> pixels;
    int width = 0;
    int height = 0;
};

// Load RGB/RGBA JPEG or PNG via stb_image (header-only)
Texture load_texture(const char* path, bool repeat = true);
// Decode without any GL calls (safe on a worker thread). The image must be
// exactly expected_width x expected_height; this is checked before decoding.
bool decode_image_rgba(const char* path, int expected_width, int expected_height,
                       ImageRGBA& out);
// Upload RGBA8 pixels with a full mipmap chain (main/GL thread only).
Texture upload_texture_rgba(const ImageRGBA& image, bool repeat = true);
void    destroy_texture(Texture& t);
void    bind_texture(const Texture& t, int unit = 0);
