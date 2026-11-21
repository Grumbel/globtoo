#pragma once
#include "camera.hpp"
#include "shader.hpp"
#include <GLES2/gl2.h>
#include <vector>

struct Countries {
    // Borders
    GLuint border_vbo = 0;
    struct Range { GLint first; GLsizei count; };
    std::vector<Range> strips;
    Shader border_shader{};

    // Political fill (triangulated)
    GLuint fill_vbo = 0;
    GLsizei fill_count = 0; // vertices (triangles * 3)
    Shader fill_shader{};

    bool borders_visible = true;
    bool fill_visible = true;
};

bool countries_init(Countries& c, const char* geojson_path);
void countries_shutdown(Countries& c);
void countries_draw(const Countries& c, const Camera& cam, float aspect);
