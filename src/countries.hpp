#pragma once
#include "camera.hpp"
#include "shader.hpp"
#include <GLES2/gl2.h>
#include <vector>
#include <string>

struct Countries {
    GLuint vbo = 0;
    // Each range is a GL_LINE_STRIP: [first_vertex, vertex_count)
    struct Range { GLint first; GLsizei count; };
    std::vector<Range> strips;
    GLsizei total_verts = 0;
    Shader shader{};
    bool visible = true;
};

// Load Natural Earth Admin-0 GeoJSON (lon/lat degrees). path may be null → try defaults.
bool countries_init(Countries& c, const char* geojson_path);
void countries_shutdown(Countries& c);
void countries_draw(const Countries& c, const Camera& cam, float aspect);
