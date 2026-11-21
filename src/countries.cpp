#include "countries.hpp"
#include "country_geometry.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

bool countries_init(Countries& c, const char* geojson_path) {
    std::vector<std::string> candidates;
    if (geojson_path && geojson_path[0]) candidates.emplace_back(geojson_path);
    candidates.emplace_back("assets/countries.geojson");
    if (const char* ad = std::getenv("GLOBE_ASSET_DIR")) {
        candidates.emplace_back(std::string(ad) + "/assets/countries.geojson");
        candidates.emplace_back(std::string(ad) + "/countries.geojson");
    }

    std::string text, used;
    for (const auto& path : candidates) {
        std::ifstream in(path, std::ios::binary);
        if (!in) continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        text = ss.str();
        if (!text.empty()) { used = path; break; }
    }
    if (text.empty()) {
        fprintf(stderr, "countries: no GeoJSON found\n");
        return false;
    }

    CountryGeometry geom;
    std::string error;
    if (!build_country_geometry(text, geom, &error)) {
        fprintf(stderr, "countries: %s (%s)\n", error.c_str(), used.c_str());
        return false;
    }

    c.strips.reserve(geom.border_strips.size());
    for (const auto& strip : geom.border_strips)
        c.strips.push_back({strip.first, strip.count});

    if (!geom.border_xyz.empty()) {
        glGenBuffers(1, &c.border_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, c.border_vbo);
        glBufferData(GL_ARRAY_BUFFER, geom.border_xyz.size() * sizeof(float),
                     geom.border_xyz.data(), GL_STATIC_DRAW);
    }

    c.fill_count = static_cast<GLsizei>(geom.fill_xyz_rgb.size() / 6);
    if (c.fill_count > 0) {
        glGenBuffers(1, &c.fill_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, c.fill_vbo);
        glBufferData(GL_ARRAY_BUFFER, geom.fill_xyz_rgb.size() * sizeof(float),
                     geom.fill_xyz_rgb.data(), GL_STATIC_DRAW);
    }

    c.border_shader = load_shader("shaders/borders.vert", "shaders/borders.frag");
    c.fill_shader = load_shader("shaders/fill.vert", "shaders/fill.frag");
    if (!c.border_shader.program || !c.fill_shader.program) {
        fprintf(stderr, "countries: shader failed\n");
        return false;
    }

    fprintf(stderr, "countries: %s — %zu features, %zu polygons, %zu rings, %d fill vertices, %zu triangulation failures\n",
            used.c_str(), geom.feature_count, geom.polygon_count, c.strips.size(),
            static_cast<int>(c.fill_count), geom.triangulation_failures);
    return true;
}

void countries_shutdown(Countries& c) {
    if (c.border_vbo) glDeleteBuffers(1, &c.border_vbo);
    if (c.fill_vbo) glDeleteBuffers(1, &c.fill_vbo);
    destroy_shader(c.border_shader);
    destroy_shader(c.fill_shader);
    c = {};
}

void countries_draw(const Countries& c, const Camera& cam, float aspect) {
    Mat4 mvp = Mat4::multiply(cam.proj_matrix(aspect), cam.view_matrix());

    // Preserve the globe's depth buffer: front hemisphere overlays pass, but
    // both fills and *lines* on the far hemisphere fail against the globe.
    // GL_CULL_FACE has no effect on GL_LINE_STRIP, so depth testing is vital.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    if (c.fill_visible && c.fill_vbo && c.fill_shader.program && c.fill_count > 0) {
        use_shader(c.fill_shader);
        glUniformMatrix4fv(c.fill_shader.u_mvp, 1, GL_FALSE, mvp.m);
        const GLint u_light = glGetUniformLocation(c.fill_shader.program, "u_light_dir");
        const GLint u_shading = glGetUniformLocation(c.fill_shader.program, "u_shading");
        if (u_light >= 0) {
            const Vec3 light = camera_headlight_dir(cam);
            glUniform3f(u_light, light.x, light.y, light.z);
        }
        if (u_shading >= 0) glUniform1f(u_shading, 1.f);

        glBindBuffer(GL_ARRAY_BUFFER, c.fill_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                              (void*)(3 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, c.fill_count);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }

    if (c.borders_visible && c.border_vbo && c.border_shader.program) {
        use_shader(c.border_shader);
        glUniformMatrix4fv(c.border_shader.u_mvp, 1, GL_FALSE, mvp.m);
        const GLint u_color = glGetUniformLocation(c.border_shader.program, "u_color");
        if (u_color >= 0) glUniform4f(u_color, 0.12f, 0.10f, 0.08f, 1.f);

        // Border radius is slightly larger than fill radius to avoid z-fighting.
        glLineWidth(1.f);
        glBindBuffer(GL_ARRAY_BUFFER, c.border_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        for (const auto& strip : c.strips)
            glDrawArrays(GL_LINE_STRIP, strip.first, strip.count);
        glDisableVertexAttribArray(0);
    }
}
