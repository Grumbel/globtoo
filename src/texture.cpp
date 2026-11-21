#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "texture.hpp"
#include <cstdio>

Texture load_texture(const char* path) {
    Texture t{};
    int channels = 0;
    // Equirectangular maps are top-down in most sources; flip so our V
    // (0 at north pole in mesh) lines up with the image after load.
    stbi_set_flip_vertically_on_load(1);

    // Always request 4 channels. JPEG is typically 3; uploading RGB with
    // default GL_UNPACK_ALIGNMENT=4 skews the image whenever width*3 is
    // not a multiple of 4. Forcing RGBA removes that entire class of bugs.
    unsigned char* data = stbi_load(path, &t.width, &t.height, &channels, 4);
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", path);
        return t;
    }

    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.width, t.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

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
