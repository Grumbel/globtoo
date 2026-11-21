#include "countries.hpp"
#include "math.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

// Same frame as create_sphere / texture UVs (see docs/COORDINATES.md):
//   lon=0 → +X, lon=90E → +Z, lat=90N → +Y
static void lonlat_to_xyz(float lon_deg, float lat_deg, float radius, float& x, float& y, float& z) {
    float lon = lon_deg * (3.14159265f / 180.f);
    float lat = lat_deg * (3.14159265f / 180.f);
    float cl = cosf(lat);
    x = cl * cosf(lon) * radius;
    y = sinf(lat) * radius;
    z = cl * sinf(lon) * radius;
}

// Classic atlas-ish palette (MAPCOLOR7 is 1..7)
static void mapcolor_rgb(int mc, float& r, float& g, float& b) {
    static const float pal[][3] = {
        {0.75f, 0.75f, 0.72f}, // 0 fallback
        {0.91f, 0.56f, 0.56f}, // 1 red-ish
        {0.55f, 0.78f, 0.55f}, // 2 green
        {0.55f, 0.65f, 0.90f}, // 3 blue
        {0.95f, 0.85f, 0.45f}, // 4 yellow
        {0.85f, 0.65f, 0.90f}, // 5 purple
        {0.55f, 0.85f, 0.85f}, // 6 cyan
        {0.95f, 0.70f, 0.45f}, // 7 orange
    };
    if (mc < 0 || mc > 7) mc = 0;
    r = pal[mc][0]; g = pal[mc][1]; b = pal[mc][2];
}

// Fan triangulation from 3D centroid on the sphere (robust for 110m countries).
static void fan_fill(const std::vector<float>& ring_lonlat,
                     std::vector<float>& out_xyz_rgb,
                     float cr, float cg, float cb, float radius) {
    int n = (int)(ring_lonlat.size() / 2);
    if (n < 3)
        return;
    if (n > 1 &&
        fabsf(ring_lonlat[0] - ring_lonlat[(n - 1) * 2]) < 1e-5f &&
        fabsf(ring_lonlat[1] - ring_lonlat[(n - 1) * 2 + 1]) < 1e-5f)
        --n;
    if (n < 3)
        return;

    // Average unit positions → centroid direction
    float cx = 0, cy = 0, cz = 0;
    std::vector<float> pts(n * 3);
    for (int i = 0; i < n; ++i) {
        float x, y, z;
        lonlat_to_xyz(ring_lonlat[i * 2], ring_lonlat[i * 2 + 1], 1.f, x, y, z);
        pts[i * 3 + 0] = x; pts[i * 3 + 1] = y; pts[i * 3 + 2] = z;
        cx += x; cy += y; cz += z;
    }
    float len = sqrtf(cx * cx + cy * cy + cz * cz);
    if (len < 1e-6f)
        return;
    cx = cx / len * radius;
    cy = cy / len * radius;
    cz = cz / len * radius;

    auto emit = [&](float x, float y, float z) {
        out_xyz_rgb.push_back(x);
        out_xyz_rgb.push_back(y);
        out_xyz_rgb.push_back(z);
        out_xyz_rgb.push_back(cr);
        out_xyz_rgb.push_back(cg);
        out_xyz_rgb.push_back(cb);
    };

    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        emit(cx, cy, cz);
        emit(pts[i * 3] * radius, pts[i * 3 + 1] * radius, pts[i * 3 + 2] * radius);
        emit(pts[j * 3] * radius, pts[j * 3 + 1] * radius, pts[j * 3 + 2] * radius);
    }
}

// Extract rings + MAPCOLOR7 per feature from Natural Earth GeoJSON
static bool parse_features(const std::string& text,
                           std::vector<float>& border_verts,
                           std::vector<Countries::Range>& strips,
                           std::vector<float>& fill_xyz_rgb) {
    const char* p = text.c_str();
    const char* end = p + text.size();

    auto skip_ws = [&]() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
    };
    auto parse_number = [&](float& out) -> bool {
        skip_ws();
        char* e = nullptr;
        out = strtof(p, &e);
        if (e == p) return false;
        p = e;
        return true;
    };
    auto try_read_position = [&](float& lon, float& lat) -> bool {
        skip_ws();
        if (*p != '[') return false;
        const char* save = p;
        ++p;
        if (!parse_number(lon)) { p = save; return false; }
        skip_ws();
        if (*p != ',') { p = save; return false; }
        ++p;
        if (!parse_number(lat)) { p = save; return false; }
        skip_ws();
        if (*p == ',') { ++p; float alt; parse_number(alt); }
        skip_ws();
        if (*p != ']') { p = save; return false; }
        ++p;
        return true;
    };

    // Split on features roughly: find each "MAPCOLOR7" then following "coordinates"
    const char* cursor = p;
    while (cursor < end) {
        const char* mc_key = strstr(cursor, "\"MAPCOLOR7\"");
        const char* coord_key = strstr(cursor, "\"coordinates\"");
        if (!coord_key)
            break;

        int mapcolor = 0;
        if (mc_key && mc_key < coord_key) {
            const char* q = mc_key + 11;
            while (q < end && *q != ':' && *q != ',') ++q;
            if (*q == ':') {
                ++q;
                mapcolor = (int)strtol(q, nullptr, 10);
            }
        }

        float cr, cg, cb;
        mapcolor_rgb(mapcolor, cr, cg, cb);

        p = coord_key + 14;
        skip_ws();
        if (*p == ':') ++p;
        skip_ws();

        // Walk coordinates arrays; first ring of each polygon = exterior → fill + border
        std::function<void(bool)> walk;
        walk = [&](bool /*may_be_ring*/) {
            skip_ws();
            if (p >= end || *p != '[') return;
            ++p;
            skip_ws();
            if (*p == '[') {
                float lon, lat;
                const char* save = p;
                if (try_read_position(lon, lat)) {
                    std::vector<float> ring;
                    ring.push_back(lon); ring.push_back(lat);
                    GLint first = (GLint)(border_verts.size() / 3);
                    float x, y, z;
                    lonlat_to_xyz(lon, lat, 1.004f, x, y, z);
                    border_verts.push_back(x); border_verts.push_back(y); border_verts.push_back(z);
                    while (true) {
                        skip_ws();
                        if (*p == ',') {
                            ++p;
                            if (!try_read_position(lon, lat)) break;
                            ring.push_back(lon); ring.push_back(lat);
                            lonlat_to_xyz(lon, lat, 1.004f, x, y, z);
                            border_verts.push_back(x); border_verts.push_back(y); border_verts.push_back(z);
                        } else break;
                    }
                    GLsizei count = (GLsizei)(border_verts.size() / 3 - first);
                    if (count >= 2)
                        strips.push_back({ first, count });
                    // Fill from exterior rings (caller treats every ring; holes get same color — ok at 110m)
                    if (ring.size() >= 6)
                        fan_fill(ring, fill_xyz_rgb, cr, cg, cb, 1.002f);
                    skip_ws();
                    if (*p == ']') ++p;
                } else {
                    p = save;
                    while (true) {
                        skip_ws();
                        if (*p == '[') walk(true);
                        skip_ws();
                        if (*p == ',') { ++p; continue; }
                        break;
                    }
                    skip_ws();
                    if (*p == ']') ++p;
                }
            } else {
                int depth = 1;
                while (p < end && depth > 0) {
                    if (*p == '[') ++depth;
                    else if (*p == ']') --depth;
                    ++p;
                }
            }
        };

        skip_ws();
        while (p < end) {
            skip_ws();
            if (*p == '[') walk(true);
            skip_ws();
            if (*p == ',') { ++p; continue; }
            if (*p == ']') { ++p; break; }
            if (p < end) ++p; else break;
        }

        cursor = p;
    }

    return !strips.empty();
}

bool countries_init(Countries& c, const char* geojson_path) {
    std::vector<std::string> candidates;
    if (geojson_path && geojson_path[0])
        candidates.emplace_back(geojson_path);
    candidates.emplace_back("assets/countries.geojson");
    if (const char* ad = std::getenv("GLOBE_ASSET_DIR")) {
        candidates.push_back(std::string(ad) + "/assets/countries.geojson");
        candidates.push_back(std::string(ad) + "/countries.geojson");
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

    std::vector<float> border_verts;
    std::vector<float> fill_verts;
    if (!parse_features(text, border_verts, c.strips, fill_verts)) {
        fprintf(stderr, "countries: parse failed (%s)\n", used.c_str());
        return false;
    }

    glGenBuffers(1, &c.border_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, c.border_vbo);
    glBufferData(GL_ARRAY_BUFFER, border_verts.size() * sizeof(float), border_verts.data(), GL_STATIC_DRAW);

    c.fill_count = (GLsizei)(fill_verts.size() / 6);
    if (c.fill_count > 0) {
        glGenBuffers(1, &c.fill_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, c.fill_vbo);
        glBufferData(GL_ARRAY_BUFFER, fill_verts.size() * sizeof(float), fill_verts.data(), GL_STATIC_DRAW);
    }

    c.border_shader = load_shader("shaders/borders.vert", "shaders/borders.frag");
    c.fill_shader = load_shader("shaders/fill.vert", "shaders/fill.frag");
    if (!c.border_shader.program) {
        fprintf(stderr, "countries: border shader failed\n");
        return false;
    }
    if (!c.fill_shader.program) {
        fprintf(stderr, "countries: fill shader failed\n");
        return false;
    }

    fprintf(stderr, "countries: %s — %d border strips, %d fill verts\n",
            used.c_str(), (int)c.strips.size(), (int)c.fill_count);
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
    Mat4 proj = cam.proj_matrix(aspect);
    Mat4 view = cam.view_matrix();
    Mat4 mvp = Mat4::multiply(proj, view);

    // Fill under borders
    if (c.fill_visible && c.fill_vbo && c.fill_shader.program && c.fill_count > 0) {
        use_shader(c.fill_shader);
        GLint u_mvp = glGetUniformLocation(c.fill_shader.program, "u_mvp");
        GLint u_light = glGetUniformLocation(c.fill_shader.program, "u_light_dir");
        GLint u_sh = glGetUniformLocation(c.fill_shader.program, "u_shading");
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp.m);
        if (u_light >= 0) {
            Vec3 L = camera_headlight_dir(cam);
            glUniform3f(u_light, L.x, L.y, L.z);
        }
        if (u_sh >= 0)
            glUniform1f(u_sh, 1.f);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.f, 1.f);

        glBindBuffer(GL_ARRAY_BUFFER, c.fill_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glDrawArrays(GL_TRIANGLES, 0, c.fill_count);
        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthFunc(GL_LESS);
    }

    if (c.borders_visible && c.border_vbo && c.border_shader.program) {
        use_shader(c.border_shader);
        GLint u_mvp = glGetUniformLocation(c.border_shader.program, "u_mvp");
        GLint u_color = glGetUniformLocation(c.border_shader.program, "u_color");
        glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp.m);
        glUniform4f(u_color, 0.15f, 0.12f, 0.10f, 1.f);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glLineWidth(1.f);

        glBindBuffer(GL_ARRAY_BUFFER, c.border_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        for (const auto& r : c.strips)
            glDrawArrays(GL_LINE_STRIP, r.first, r.count);
        glDisableVertexAttribArray(0);
        glDepthFunc(GL_LESS);
    }
}
