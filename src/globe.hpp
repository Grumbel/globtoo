#pragma once
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "map_view.hpp"

struct Globe {
    Mesh    mesh;
    Shader  shader;
    Texture texture;
    bool    shading = true;
    bool    texture_visible = true;  // Independent of political country colors
};

bool globe_init(Globe& g, const char* texture_path);
void globe_shutdown(Globe& g);
// When texture is disabled, render the solid ocean base instead.
void globe_draw(const Globe& g, const Camera& cam, float aspect,
                const MapView& map);
