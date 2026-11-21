#include "country_geometry.hpp"
#include "geocoordinates.hpp"

#define GEOS_USE_ONLY_R_API
#include <geos_c.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
constexpr double pi = 3.14159265358979323846;
// Fills and outlines MUST sit on the same surface. At close zoom a 0.001
// radius difference moves the border several screen pixels relative to fill.
// The depth buffer contains only the base globe; borders are drawn last.
constexpr double political_radius = 1.0015;

// The same densely-sampled ring is a constraint in the triangulation and is
// uploaded as a border strip. Never subdivide the outline independently.
constexpr double max_boundary_edge_degrees = 0.5;
// Curved triangles need sufficient tessellation for the nearest camera zoom.
constexpr double max_triangle_edge_degrees = 2.5;

struct Point { double x, y; }; // unwrapped longitude, latitude (degrees)
using Ring = std::vector<Point>;
using Polygon = std::vector<Ring>; // shell, followed by holes
using MultiPolygon = std::vector<Polygon>;

// A small JSON reader: don't assume property order, whitespace, formatting or
// number of coordinate levels. Unknown properties are skipped, not searched
// for using strstr (which can cross feature boundaries).
class Json {
public:
    explicit Json(std::string_view input) : input_(input) {}

    void ws() {
        while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\n' ||
               input_[pos_] == '\r' || input_[pos_] == '\t')) ++pos_;
    }
    bool take(char c) {
        ws();
        if (pos_ < input_.size() && input_[pos_] == c) { ++pos_; return true; }
        return false;
    }
    bool expect(char c) { return take(c); }
    bool end() { ws(); return pos_ == input_.size(); }

    bool string(std::string& result) {
        ws();
        if (!take('"')) return false;
        result.clear();
        while (pos_ < input_.size()) {
            char ch = input_[pos_++];
            if (ch == '"') return true;
            if (ch != '\\') { result += ch; continue; }
            if (pos_ >= input_.size()) return false;
            ch = input_[pos_++];
            switch (ch) {
                case '"': case '\\': case '/': result += ch; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'b': case 'f': result += ' '; break;
                case 'u':
                    if (input_.size() - pos_ < 4) return false;
                    pos_ += 4; // only names are read; escaped Unicode isn't needed for geometry
                    result += '?';
                    break;
                default: return false;
            }
        }
        return false;
    }
    bool number(double& n) {
        ws();
        if (pos_ >= input_.size()) return false;
        // strtod needs a terminated string; input is a view of the owned
        // GeoJSON file, so copy the at-most-32-byte numeric token instead.
        std::size_t begin = pos_;
        while (pos_ < input_.size() &&
               (input_[pos_] == '-' || input_[pos_] == '+' || input_[pos_] == '.' ||
                input_[pos_] == 'e' || input_[pos_] == 'E' ||
                (input_[pos_] >= '0' && input_[pos_] <= '9'))) ++pos_;
        if (pos_ == begin || pos_ - begin > 64) return false;
        std::string token(input_.substr(begin, pos_ - begin));
        char* end = nullptr;
        n = strtod(token.c_str(), &end);
        return end == token.c_str() + token.size() && std::isfinite(n);
    }
    bool skip(unsigned depth = 0) {
        if (depth > 64) return false;
        ws();
        if (take('{')) {
            if (take('}')) return true;
            do {
                std::string key;
                if (!string(key) || !take(':') || !skip(depth + 1)) return false;
            } while (take(','));
            return take('}');
        }
        if (take('[')) {
            if (take(']')) return true;
            do { if (!skip(depth + 1)) return false; } while (take(','));
            return take(']');
        }
        if (take('"')) {
            // Skip quoted string, including escaped quote/backslash.
            while (pos_ < input_.size()) {
                char c = input_[pos_++];
                if (c == '"') return true;
                if (c == '\\' && pos_ < input_.size()) ++pos_;
            }
            return false;
        }
        for (auto literal : {"true", "false", "null"}) {
            if (input_.substr(pos_, std::char_traits<char>::length(literal)) == literal) {
                pos_ += std::char_traits<char>::length(literal);
                return true;
            }
        }
        double ignored;
        return number(ignored);
    }
    bool slice(std::string_view& value) {
        ws();
        const auto begin = pos_;
        if (!skip()) return false;
        value = input_.substr(begin, pos_ - begin);
        return true;
    }
private:
    std::string_view input_;
    std::size_t pos_ = 0;
};

bool read_point(Json& j, Point& p) {
    if (!j.take('[') || !j.number(p.x) || !j.take(',') || !j.number(p.y)) return false;
    // GeoJSON allows optional altitude and additional dimensions.
    while (j.take(',')) { double ignored; if (!j.number(ignored)) return false; }
    return j.take(']');
}
bool read_ring(Json& j, Ring& ring) {
    if (!j.take('[')) return false;
    if (!j.take(']')) {
        do { Point p; if (!read_point(j, p)) return false; ring.push_back(p); }
        while (j.take(','));
        if (!j.take(']')) return false;
    }
    return true;
}
bool read_polygon(Json& j, Polygon& poly) {
    if (!j.take('[')) return false;
    if (!j.take(']')) {
        do { Ring ring; if (!read_ring(j, ring)) return false; poly.push_back(std::move(ring)); }
        while (j.take(','));
        if (!j.take(']')) return false;
    }
    return true;
}
bool read_coordinates(std::string_view json, const std::string& type, MultiPolygon& result) {
    Json j(json);
    if (type == "Polygon") {
        Polygon polygon;
        if (!read_polygon(j, polygon)) return false;
        result.push_back(std::move(polygon));
    } else if (type == "MultiPolygon") {
        if (!j.take('[')) return false;
        if (!j.take(']')) {
            do {
                Polygon polygon;
                if (!read_polygon(j, polygon)) return false;
                result.push_back(std::move(polygon));
            } while (j.take(','));
            if (!j.take(']')) return false;
        }
    } else return true; // unsupported geometry types are harmless
    return j.end();
}

bool parse_properties(Json& j, int& mapcolor, std::string& name) {
    if (!j.take('{')) return false;
    if (j.take('}')) return true;
    do {
        std::string key;
        if (!j.string(key) || !j.take(':')) return false;
        if (key == "MAPCOLOR7") {
            double number;
            if (j.number(number)) mapcolor = (int)number;
            else if (!j.skip()) return false;
        } else if (key == "NAME") {
            if (!j.string(name) && !j.skip()) return false;
        } else if (!j.skip()) return false;
    } while (j.take(','));
    return j.take('}');
}
bool parse_geometry(Json& j, MultiPolygon& polygons) {
    if (!j.take('{')) return j.skip(); // null geometry
    std::string type;
    std::string_view coordinates;
    if (!j.take('}')) {
        do {
            std::string key;
            if (!j.string(key) || !j.take(':')) return false;
            if (key == "type") {
                if (!j.string(type)) return false;
            } else if (key == "coordinates") {
                if (!j.slice(coordinates)) return false;
            } else if (!j.skip()) return false;
        } while (j.take(','));
        if (!j.take('}')) return false;
    }
    return coordinates.empty() || read_coordinates(coordinates, type, polygons);
}

unsigned hash_name(const std::string& name) {
    unsigned hash = 2166136261u;
    for (unsigned char c : name) { hash ^= c; hash *= 16777619u; }
    return hash;
}
std::array<float, 3> country_color(int mapcolor, const std::string& name) {
    static constexpr float palette[7][3] = {
        {0.93f, 0.55f, 0.52f}, {0.45f, 0.72f, 0.48f},
        {0.95f, 0.82f, 0.40f}, {0.55f, 0.62f, 0.88f},
        {0.90f, 0.58f, 0.78f}, {0.40f, 0.78f, 0.78f},
        {0.95f, 0.68f, 0.38f}
    };
    const unsigned idx = mapcolor >= 1 && mapcolor <= 7 ?
        static_cast<unsigned>(mapcolor - 1) : hash_name(name) % 7;
    return {palette[idx][0], palette[idx][1], palette[idx][2]};
}

// Unwrap a polygon locally, not the entire map. GEOS triangulates in 2D;
// a ring straddling +/-180 must be near 180 in that plane, not 360 deg wide.
void unwrap(Ring& ring, double reference, bool outer) {
    if (ring.empty()) return;
    if (ring.size() > 1 && std::abs(ring.front().x - ring.back().x) < 1e-8 &&
        std::abs(ring.front().y - ring.back().y) < 1e-8) ring.pop_back();
    if (ring.empty()) return;
    if (!outer) ring[0].x += 360 * std::round((reference - ring[0].x) / 360);
    for (std::size_t i = 1; i < ring.size(); ++i) {
        // Antarctic data has an intentional +180 -> -180 cut along the south
        // pole. Preserve that planar edge: unwrapping it would turn the
        // 360-degree polar cap into a giant closing chord across the globe.
        const bool polar_cut = std::abs(ring[i - 1].y) >= 89.999 &&
                               std::abs(ring[i].y) >= 89.999 &&
                               std::abs(ring[i - 1].x - ring[i].x) >= 359.999;
        if (!polar_cut)
            ring[i].x += 360 * std::round((ring[i - 1].x - ring[i].x) / 360);
    }
    if (outer) return;
    double mean = 0;
    for (auto p : ring) mean += p.x;
    mean /= ring.size();
    const double shift = 360 * std::round((reference - mean) / 360);
    for (auto& p : ring) p.x += shift;
}

std::array<double, 3> sphere(Point p, double radius) {
    return geo::lonlat_to_xyz(p.x, p.y, radius);
}

void densify_ring(Ring& ring) {
    if (ring.size() < 2) return;
    Ring points;
    points.reserve(ring.size());
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const auto a = ring[i], b = ring[(i + 1) % ring.size()];
        const int segments = std::max(1, static_cast<int>(std::ceil(
            std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)) /
            max_boundary_edge_degrees)));
        points.push_back(a);
        for (int j = 1; j < segments; ++j) {
            const double t = static_cast<double>(j) / segments;
            points.push_back({a.x + (b.x - a.x) * t,
                              a.y + (b.y - a.y) * t});
        }
    }
    ring = std::move(points);
}

void emit_fill_triangle(CountryGeometry& dst, Point a, Point b, Point c,
                        const std::array<float, 3>& color, int depth = 0) {
    const auto len2 = [](Point p, Point q) {
        const double dx = p.x - q.x, dy = p.y - q.y;
        return dx * dx + dy * dy;
    };
    const double ab = len2(a, b), bc = len2(b, c), ca = len2(c, a);
    constexpr double limit2 = max_triangle_edge_degrees * max_triangle_edge_degrees;
    if (depth < 20 && std::max({ab, bc, ca}) > limit2) {
        if (ab >= bc && ab >= ca) {
            Point mid{(a.x + b.x) * .5, (a.y + b.y) * .5};
            emit_fill_triangle(dst, a, mid, c, color, depth + 1);
            emit_fill_triangle(dst, mid, b, c, color, depth + 1);
        } else if (bc >= ca) {
            Point mid{(b.x + c.x) * .5, (b.y + c.y) * .5};
            emit_fill_triangle(dst, a, b, mid, color, depth + 1);
            emit_fill_triangle(dst, a, mid, c, color, depth + 1);
        } else {
            Point mid{(c.x + a.x) * .5, (c.y + a.y) * .5};
            emit_fill_triangle(dst, a, b, mid, color, depth + 1);
            emit_fill_triangle(dst, mid, b, c, color, depth + 1);
        }
        return;
    }
    auto p = sphere(a, political_radius);
    auto q = sphere(b, political_radius);
    auto r = sphere(c, political_radius);
    const double ux = q[0] - p[0], uy = q[1] - p[1], uz = q[2] - p[2];
    const double vx = r[0] - p[0], vy = r[1] - p[1], vz = r[2] - p[2];
    const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const double outward = nx * (p[0] + q[0] + r[0]) +
                           ny * (p[1] + q[1] + r[1]) + nz * (p[2] + q[2] + r[2]);
    if (std::abs(outward) < 1e-12) return;
    if (outward < 0) std::swap(q, r);
    for (auto& point : {p, q, r}) {
        for (double x : point) dst.fill_xyz_rgb.push_back(static_cast<float>(x));
        for (float channel : color) dst.fill_xyz_rgb.push_back(channel);
    }
}

void emit_border(CountryGeometry& dst, const Ring& ring) {
    if (ring.size() < 2) return;
    const int first = static_cast<int>(dst.border_xyz.size() / 3);
    // Exactly the same vertices/edges as the triangulator's constrained
    // boundary; no separately interpolated curve and no extra radial lift.
    for (std::size_t i = 0; i <= ring.size(); ++i) {
        const auto v = sphere(ring[i % ring.size()], political_radius);
        for (auto x : v) dst.border_xyz.push_back(static_cast<float>(x));
    }
    dst.border_strips.push_back({first, static_cast<int>(ring.size() + 1)});
}

GEOSGeometry* make_linear_ring(GEOSContextHandle_t ctx, const Ring& ring) {
    if (ring.size() < 3) return nullptr;
    auto* seq = GEOSCoordSeq_create_r(ctx, static_cast<unsigned>(ring.size() + 1), 2);
    if (!seq) return nullptr;
    for (std::size_t i = 0; i <= ring.size(); ++i) {
        const Point p = ring[i % ring.size()];
        if (!GEOSCoordSeq_setX_r(ctx, seq, static_cast<unsigned>(i), p.x) ||
            !GEOSCoordSeq_setY_r(ctx, seq, static_cast<unsigned>(i), p.y)) {
            GEOSCoordSeq_destroy_r(ctx, seq);
            return nullptr;
        }
    }
    auto* result = GEOSGeom_createLinearRing_r(ctx, seq);
    if (!result) GEOSCoordSeq_destroy_r(ctx, seq);
    return result; // owns seq on success
}

GEOSGeometry* make_polygon(GEOSContextHandle_t ctx, const Polygon& polygon) {
    if (polygon.empty()) return nullptr;
    GEOSGeometry* shell = make_linear_ring(ctx, polygon[0]);
    if (!shell) return nullptr;
    std::vector<GEOSGeometry*> holes;
    for (std::size_t i = 1; i < polygon.size(); ++i) {
        if (auto* ring = make_linear_ring(ctx, polygon[i])) holes.push_back(ring);
    }
    GEOSGeometry* shape = GEOSGeom_createPolygon_r(ctx, shell, holes.data(),
                                                    static_cast<unsigned>(holes.size()));
    if (!shape) {
        GEOSGeom_destroy_r(ctx, shell);
        for (auto* hole : holes) GEOSGeom_destroy_r(ctx, hole);
    }
    return shape;
}

void collect_triangles(GEOSContextHandle_t ctx, const GEOSGeometry* geometry,
                       CountryGeometry& dst, const std::array<float, 3>& color) {
    if (!geometry) return;
    const int type = GEOSGeomTypeId_r(ctx, geometry);
    if (type == GEOS_POLYGON) {
        const auto* shell = GEOSGetExteriorRing_r(ctx, geometry);
        const auto* coords = GEOSGeom_getCoordSeq_r(ctx, shell);
        unsigned n = 0;
        if (!coords || !GEOSCoordSeq_getSize_r(ctx, coords, &n) || n < 4) return;
        // CDT output polygons are triangles (four points including closure).
        if (n != 4) return;
        Point pts[3];
        for (unsigned i = 0; i < 3; ++i) {
            if (!GEOSCoordSeq_getX_r(ctx, coords, i, &pts[i].x) ||
                !GEOSCoordSeq_getY_r(ctx, coords, i, &pts[i].y)) return;
        }
        emit_fill_triangle(dst, pts[0], pts[1], pts[2], color);
    } else if (type == GEOS_GEOMETRYCOLLECTION || type == GEOS_MULTIPOLYGON) {
        const int n = GEOSGetNumGeometries_r(ctx, geometry);
        for (int i = 0; i < n; ++i)
            collect_triangles(ctx, GEOSGetGeometryN_r(ctx, geometry, i), dst, color);
    }
}

bool copy_geos_ring(GEOSContextHandle_t ctx, const GEOSGeometry* geometry,
                    Ring& ring) {
    if (!geometry) return false;
    const auto* coords = GEOSGeom_getCoordSeq_r(ctx, geometry);
    unsigned n = 0;
    if (!coords || !GEOSCoordSeq_getSize_r(ctx, coords, &n) || n < 4)
        return false;
    ring.clear();
    ring.reserve(n - 1);
    for (unsigned i = 0; i < n - 1; ++i) {
        Point p{};
        if (!GEOSCoordSeq_getX_r(ctx, coords, i, &p.x) ||
            !GEOSCoordSeq_getY_r(ctx, coords, i, &p.y)) return false;
        ring.push_back(p);
    }
    return true;
}

// Only the repaired, *valid* polygon determines the border. Previously the
// original ring was drawn even if GEOSMakeValid had changed the fill shape.
// Densifying those rings BEFORE the constrained triangulation ensures every
// border segment is literally an edge of the filled triangle mesh.
void triangulate_valid_geometry(GEOSContextHandle_t ctx, const GEOSGeometry* geometry,
                                CountryGeometry& dst, const std::array<float, 3>& color) {
    if (!geometry || GEOSisEmpty_r(ctx, geometry)) return;
    const int type = GEOSGeomTypeId_r(ctx, geometry);
    if (type == GEOS_POLYGON) {
        Polygon sampled;
        Ring shell;
        if (!copy_geos_ring(ctx, GEOSGetExteriorRing_r(ctx, geometry), shell)) {
            ++dst.triangulation_failures;
            return;
        }
        densify_ring(shell);
        sampled.push_back(std::move(shell));
        const int holes = GEOSGetNumInteriorRings_r(ctx, geometry);
        for (int i = 0; i < holes; ++i) {
            Ring ring;
            if (!copy_geos_ring(ctx, GEOSGetInteriorRingN_r(ctx, geometry, i), ring)) {
                ++dst.triangulation_failures;
                return;
            }
            densify_ring(ring);
            sampled.push_back(std::move(ring));
        }
        GEOSGeometry* densified = make_polygon(ctx, sampled);
        if (!densified) { ++dst.triangulation_failures; return; }
        GEOSGeometry* triangles = GEOSConstrainedDelaunayTriangulation_r(ctx, densified);
        if (triangles) {
            for (const auto& ring : sampled) emit_border(dst, ring);
            collect_triangles(ctx, triangles, dst, color);
            GEOSGeom_destroy_r(ctx, triangles);
        } else {
            ++dst.triangulation_failures;
        }
        GEOSGeom_destroy_r(ctx, densified);
    } else if (type == GEOS_MULTIPOLYGON || type == GEOS_GEOMETRYCOLLECTION) {
        const int count = GEOSGetNumGeometries_r(ctx, geometry);
        for (int i = 0; i < count; ++i)
            triangulate_valid_geometry(ctx, GEOSGetGeometryN_r(ctx, geometry, i), dst, color);
    }
}

void triangulate_polygon(GEOSContextHandle_t ctx, GEOSGeometry* polygon,
                         CountryGeometry& dst, const std::array<float, 3>& color) {
    if (!polygon) { ++dst.triangulation_failures; return; }
    GEOSGeometry* valid = polygon;
    if (GEOSisValid_r(ctx, polygon) != 1) {
        valid = GEOSMakeValid_r(ctx, polygon);
        if (!valid) { ++dst.triangulation_failures; return; }
    }
    triangulate_valid_geometry(ctx, valid, dst, color);
    if (valid != polygon) GEOSGeom_destroy_r(ctx, valid);
}

void process_feature(GEOSContextHandle_t ctx, MultiPolygon& polygons, int mapcolor,
                     const std::string& name, CountryGeometry& output) {
    ++output.feature_count;
    const auto color = country_color(mapcolor, name);
    for (auto& poly : polygons) {
        if (poly.empty()) continue;
        unwrap(poly[0], 0, true);
        if (poly[0].size() < 3) continue;
        double reference = 0;
        for (auto p : poly[0]) reference += p.x;
        reference /= poly[0].size();
        for (std::size_t i = 1; i < poly.size(); ++i) unwrap(poly[i], reference, false);
        ++output.polygon_count;
        GEOSGeometry* shape = make_polygon(ctx, poly);
        triangulate_polygon(ctx, shape, output, color);
        if (shape) GEOSGeom_destroy_r(ctx, shape);
    }
}

bool parse_feature(Json& j, GEOSContextHandle_t ctx, CountryGeometry& output) {
    if (!j.take('{')) return false;
    int mapcolor = 0;
    std::string name;
    MultiPolygon polygons;
    if (!j.take('}')) {
        do {
            std::string key;
            if (!j.string(key) || !j.take(':')) return false;
            if (key == "properties") {
                if (!parse_properties(j, mapcolor, name)) return false;
            } else if (key == "geometry") {
                if (!parse_geometry(j, polygons)) return false;
            } else if (!j.skip()) return false;
        } while (j.take(','));
        if (!j.take('}')) return false;
    }
    process_feature(ctx, polygons, mapcolor, name, output);
    return true;
}

bool parse_features(Json& j, GEOSContextHandle_t ctx, CountryGeometry& output) {
    if (!j.take('[')) return false;
    if (!j.take(']')) {
        do { if (!parse_feature(j, ctx, output)) return false; } while (j.take(','));
        if (!j.take(']')) return false;
    }
    return true;
}

} // namespace

bool build_country_geometry(std::string_view geojson, CountryGeometry& out,
                            std::string* error) {
    out = {};
    Json j(geojson);
    const auto ctx = GEOS_init_r();
    if (!ctx) { if (error) *error = "GEOS_init_r failed"; return false; }
    bool ok = j.take('{');
    bool has_features = false;
    if (ok && !j.take('}')) {
        do {
            std::string key;
            if (!j.string(key) || !j.take(':')) { ok = false; break; }
            if (key == "features") {
                has_features = true;
                if (!parse_features(j, ctx, out)) { ok = false; break; }
            } else if (!j.skip()) { ok = false; break; }
        } while (j.take(','));
        if (ok) ok = j.take('}');
    }
    ok = ok && j.end() && has_features;
    GEOS_finish_r(ctx);
    if (!ok) {
        out = {};
        if (error) *error = "invalid or unsupported GeoJSON FeatureCollection";
    }
    return ok;
}
