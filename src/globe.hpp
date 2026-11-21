#pragma once
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "camera.hpp"

struct Globe {
    Mesh    mesh;
    Shader  shader;
    Texture texture;
    bool    shading = true;
};

bool globe_init(Globe& g, const char* texture_path);
void globe_shutdown(Globe& g);
// textured=false → solid ocean (political desk-globe base)
void globe_draw(const Globe& g, const Camera& cam, float aspect, bool textured = true);
