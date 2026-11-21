#pragma once
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "camera.hpp"

struct Globe {
    Mesh    mesh;
    Shader  shader;
    Texture texture;
};

bool globe_init(Globe& g, const char* texture_path);
void globe_shutdown(Globe& g);
void globe_draw(const Globe& g, const Camera& cam, float aspect);
