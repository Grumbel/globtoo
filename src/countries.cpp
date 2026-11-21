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

static void lonlat_to_xyz(float lon_deg, float lat_deg, float radius, float& x, float& y, float& z) {
    float lon = lon_deg * (3.14159265f / 180.f);
    float lat = lat_deg * (3.14159265f / 180.f);
    float cl = cosf(lat);
    x = cl * cosf(lon) * radius;
    y = sinf(lat) * radius;
    z = cl * sinf(lon) * radius;
}

// Very small GeoJSON coordinate extractor: find nested [lon, lat] pairs.
// Handles Polygon / MultiPolygon rings (arrays of positions).
// Not a full JSON parser — good enough for Natural Earth Admin-0.
static bool parse_geojson_rings(const std::string& text, std::vector<float>& verts,
                                std::vector<Countries::Range>& strips) {
    const char* p = text.c_str();
    const char* end = p + text.size();

    auto skip_ws = [&]() {
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t'))
            ++p;
    };
    auto match = [&](const char* s) -> bool {
        skip_ws();
        size_t n = strlen(s);
        if ((size_t)(end - p) < n || strncmp(p, s, n) != 0)
            return false;
        p += n;
        return true;
    };
    auto parse_number = [&](float& out) -> bool {
        skip_ws();
        char* e = nullptr;
        out = strtof(p, &e);
        if (e == p)
            return false;
        p = e;
        return true;
    };

    // Scan for "coordinates" keys and walk the following array structure.
    while (p < end) {
        const char* key = strstr(p, "\"coordinates\"");
        if (!key)
            break;
        p = key + 14;
        skip_ws();
        if (!match(":"))
            continue;
        skip_ws();
        if (*p != '[')
            continue;
        ++p; // enter coordinates value

        // coordinates is either:
        //   Polygon:      [ ring, ring, ... ]  ring = [ [lon,lat], ... ]
        //   MultiPolygon: [ polygon, ... ]
        // We detect depth of nested arrays until we see numbers.

        std::vector<int> array_depth_stack;
        // Heuristic: walk and collect rings of [lon,lat] pairs.
        // A ring starts when we see [[number
        auto try_read_position = [&](float& lon, float& lat) -> bool {
            skip_ws();
            if (*p != '[')
                return false;
            const char* save = p;
            ++p;
            if (!parse_number(lon)) { p = save; return false; }
            skip_ws();
            if (*p != ',') { p = save; return false; }
            ++p;
            if (!parse_number(lat)) { p = save; return false; }
            skip_ws();
            // optional altitude
            if (*p == ',') {
                ++p;
                float alt;
                parse_number(alt);
            }
            skip_ws();
            if (*p != ']') { p = save; return false; }
            ++p;
            return true;
        };

        // Recursive-ish ring collector using bracket counting
        std::function<void(int)> walk;
        walk = [&](int depth) {
            skip_ws();
            if (p >= end || *p != '[')
                return;
            ++p; // consume '['

            skip_ws();
            // Peek: is first element a number-bearing position or another array?
            if (*p == '[') {
                // Could be ring of positions or nested polygons
                // Try position first
                float lon, lat;
                const char* save = p;
                if (try_read_position(lon, lat)) {
                    // This is a ring: first point already read
                    GLint first = (GLint)(verts.size() / 3);
                    float x, y, z;
                    // Slightly above unit sphere to avoid z-fight
                    lonlat_to_xyz(lon, lat, 1.004f, x, y, z);
                    verts.push_back(x); verts.push_back(y); verts.push_back(z);
                    while (true) {
                        skip_ws();
                        if (*p == ',') {
                            ++p;
                            if (!try_read_position(lon, lat))
                                break;
                            lonlat_to_xyz(lon, lat, 1.004f, x, y, z);
                            verts.push_back(x); verts.push_back(y); verts.push_back(z);
                        } else {
                            break;
                        }
                    }
                    GLsizei count = (GLsizei)(verts.size() / 3 - first);
                    if (count >= 2)
                        strips.push_back({ first, count });
                    skip_ws();
                    if (*p == ']')
                        ++p;
                } else {
                    p = save;
                    // Nested arrays (MultiPolygon)
                    while (true) {
                        skip_ws();
                        if (*p == '[')
                            walk(depth + 1);
                        skip_ws();
                        if (*p == ',') {
                            ++p;
                            continue;
                        }
                        break;
                    }
                    skip_ws();
                    if (*p == ']')
                        ++p;
                }
            } else {
                // Unexpected
                int depth2 = 1;
                while (p < end && depth2 > 0) {
                    if (*p == '[') ++depth2;
                    else if (*p == ']') --depth2;
                    ++p;
                }
            }

            skip_ws();
            // If parent expects more sibling arrays after this one, caller handles commas
        };

        // Top-level coordinates array content
        skip_ws();
        while (p < end) {
            skip_ws();
            if (*p == '[')
                walk(0);
            skip_ws();
            if (*p == ',') {
                ++p;
                continue;
            }
            if (*p == ']') {
                ++p;
                break;
            }
            // skip junk
            if (p < end)
                ++p;
            else
                break;
        }
    }

    return !strips.empty();
}

bool countries_init(Countries& c, const char* geojson_path) {
    std::vector<std::string> candidates;
    if (geojson_path && geojson_path[0])
        candidates.push_back(geojson_path);
    candidates.push_back("assets/countries.geojson");
    if (const char* ad = std::getenv("GLOBE_ASSET_DIR")) {
        candidates.push_back(std::string(ad) + "/assets/countries.geojson");
        candidates.push_back(std::string(ad) + "/countries.geojson");
    }

    std::string text;
    std::string used;
    for (const auto& path : candidates) {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        text = ss.str();
        if (!text.empty()) {
            used = path;
            break;
        }
    }
    if (text.empty()) {
        fprintf(stderr, "countries: no GeoJSON found (tried assets/countries.geojson)\n");
        return false;
    }

    std::vector<float> verts;
    if (!parse_geojson_rings(text, verts, c.strips)) {
        fprintf(stderr, "countries: failed to parse rings from %s\n", used.c_str());
        return false;
    }
    c.total_verts = (GLsizei)(verts.size() / 3);

    glGenBuffers(1, &c.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    c.shader = load_shader("shaders/borders.vert", "shaders/borders.frag");
    if (!c.shader.program) {
        fprintf(stderr, "countries: border shader failed\n");
        return false;
    }

    fprintf(stderr, "countries: %s — %d strips, %d verts\n",
            used.c_str(), (int)c.strips.size(), (int)c.total_verts);
    c.visible = true;
    return true;
}

void countries_shutdown(Countries& c) {
    if (c.vbo)
        glDeleteBuffers(1, &c.vbo);
    destroy_shader(c.shader);
    c = {};
}

void countries_draw(const Countries& c, const Camera& cam, float aspect) {
    if (!c.visible || !c.vbo || !c.shader.program)
        return;

    Mat4 proj = cam.proj_matrix(aspect);
    Mat4 view = cam.view_matrix();
    Mat4 mvp = Mat4::multiply(proj, view);

    use_shader(c.shader);
    GLint u_mvp = glGetUniformLocation(c.shader.program, "u_mvp");
    GLint u_color = glGetUniformLocation(c.shader.program, "u_color");
    glUniformMatrix4fv(u_mvp, 1, GL_FALSE, mvp.m);
    glUniform4f(u_color, 0.95f, 0.92f, 0.75f, 1.f); // warm border line

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    // GLES2 often has no wide lines; 1.0 is portable
    glLineWidth(1.f);

    glBindBuffer(GL_ARRAY_BUFFER, c.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

    for (const auto& r : c.strips)
        glDrawArrays(GL_LINE_STRIP, r.first, r.count);

    glDisableVertexAttribArray(0);
    glDepthFunc(GL_LESS);
}
