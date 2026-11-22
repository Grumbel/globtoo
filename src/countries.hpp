#pragma once
#include "camera.hpp"
#include "country_geometry.hpp"
#include "map_view.hpp"
#include "shader.hpp"
#include <GLES2/gl2.h>
#include <vector>

struct CountryMeshGPU {
    GLuint border_vbo = 0;
    GLsizei border_count = 0;
    GLuint fill_vbo = 0;
    GLsizei fill_count = 0;
    std::vector<CountryGeometry::Label> labels;
    bool available() const { return border_vbo && fill_vbo && fill_count > 0; }
};

struct Countries {
    CountryMeshGPU low;  // Natural Earth 50m
    CountryMeshGPU high; // Natural Earth 10m
    bool using_high = false;
    Shader border_shader{};
    Shader fill_shader{};
    bool borders_visible = true;
    bool fill_visible = true;
    bool names_visible = true;
};

// Explicit path forces a single custom GeoJSON dataset (useful for development).
// With nullptr, load precompiled 50m + 10m assets, or legacy GeoJSON fallback.
bool countries_init(Countries& c, const char* geojson_path);
void countries_shutdown(Countries& c);
void countries_draw(Countries& c, const Camera& cam, float aspect,
                    const MapView& map, bool shading);
