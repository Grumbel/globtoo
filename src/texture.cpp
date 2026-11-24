// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "texture.hpp"
#include <cstdio>

namespace {
void upload_rgba(Texture& t, const unsigned char* data, bool repeat) {
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width, t.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
}
} // namespace

bool decode_image_rgba(const char* path, int expected_width, int expected_height,
                       ImageRGBA& out) {
    out = {};
    // stb_image keeps its flip flag and failure reason thread-local
    // (STBI_THREAD_LOCAL), and the flip default is off, as required here.
    int width = 0, height = 0, channels = 0;
    if (!stbi_info(path, &width, &height, &channels) ||
        width != expected_width || height != expected_height)
        return false;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) return false;
    out.pixels.assign(data, data + static_cast<size_t>(width) * height * 4);
    out.width = width;
    out.height = height;
    stbi_image_free(data);
    return true;
}

Texture upload_texture_rgba(const ImageRGBA& image, bool repeat) {
    Texture t{};
    if (image.pixels.size() != static_cast<size_t>(image.width) * image.height * 4 ||
        image.pixels.empty())
        return t;
    t.width = image.width;
    t.height = image.height;
    upload_rgba(t, image.pixels.data(), repeat);
    return t;
}

Texture load_texture(const char* path, bool repeat) {
    Texture t{};
    int channels = 0;
    // Do NOT flip: equirectangular has north at the top of the image.
    // Our sphere uses v=0 at the north pole. glTexImage2D puts the first
    // memory row at v=0, and stbi loads the top of the file first, so
    // north lands on the north pole without a flip.
    stbi_set_flip_vertically_on_load(0);

    // Always request 4 channels. JPEG is typically 3; uploading RGB with
    // default GL_UNPACK_ALIGNMENT=4 skews the image whenever width*3 is
    // not a multiple of 4. Forcing RGBA removes that entire class of bugs.
    // Inspect dimensions before decoding: a 21600x10800 JPEG would cost
    // ~890 MiB just for the temporary RGBA buffer. Such sources belong in
    // the build-time tiler, not the single-texture GLES2 upload path.
    int source_width=0, source_height=0;
    if (!stbi_info(path, &source_width, &source_height, &channels)) {
        fprintf(stderr, "Failed to inspect texture: %s\n", path);
        return t;
    }
    GLint max_size=0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_size);
    if (source_width>max_size || source_height>max_size) {
        fprintf(stderr, "Texture %s (%dx%d) exceeds GLES2 max %d: "
                        "prepare it as raster tiles instead.\n",
                path,source_width,source_height,max_size);
        return t;
    }
    unsigned char* data = stbi_load(path, &t.width, &t.height, &channels, 4);
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", path);
        return t;
    }

    upload_rgba(t, data, repeat);
    stbi_image_free(data);
    fprintf(stderr, "Loaded texture %s (%dx%d, requested RGBA, source had %d ch)\n",
            path, t.width, t.height, channels);
    return t;
}

void destroy_texture(Texture& t) {
    if (t.id) glDeleteTextures(1, &t.id);
    t = {};
}

void bind_texture(const Texture& t, int unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, t.id);
}
