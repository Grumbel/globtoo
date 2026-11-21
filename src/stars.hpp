#pragma once
#include "camera.hpp"
#include "shader.hpp"
#include <GLES2/gl2.h>

struct Stars {
    GLuint vbo = 0;
    GLsizei count = 0;
    Shader shader{};
};

bool stars_init(Stars& s);
void stars_shutdown(Stars& s);
void stars_draw(const Stars& s, const Camera& cam, float aspect);
