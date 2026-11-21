#include "country_geometry.hpp"

#include <algorithm>
#include <array>
#ifdef NDEBUG
#undef NDEBUG // The tests must still check assertions in Release/Nix builds.
#endif
#include <cassert>
#include <cmath>
#include <string>

namespace {
constexpr float pi = 3.14159265358979323846f;

void verify_triangles(const CountryGeometry& geom) {
    assert(!geom.fill_xyz_rgb.empty());
    assert(geom.fill_xyz_rgb.size() % 18 == 0);
    int red = 0, green = 0, pink = 0;
    for (std::size_t i = 0; i < geom.fill_xyz_rgb.size(); i += 18) {
        std::array<std::array<float, 3>, 3> p{};
        for (int n = 0; n < 3; ++n) {
            for (int k = 0; k < 3; ++k) p[n][k] = geom.fill_xyz_rgb[i + n * 6 + k];
            const auto& v = p[n];
            const float radius = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            assert(std::abs(radius - 1.0015f) < 1e-5f);
        }
        const auto v = [&](int x, int y, int k) { return p[y][k] - p[x][k]; };
        const std::array<float, 3> cross{
            v(0, 1, 1) * v(0, 2, 2) - v(0, 1, 2) * v(0, 2, 1),
            v(0, 1, 2) * v(0, 2, 0) - v(0, 1, 0) * v(0, 2, 2),
            v(0, 1, 0) * v(0, 2, 1) - v(0, 1, 1) * v(0, 2, 0)
        };
        const float outward = cross[0] * p[0][0] + cross[1] * p[0][1] + cross[2] * p[0][2];
        assert(outward > -1e-6f); // GL_CCW front-facing relative to globe

        // All generated triangle edges should be short enough not to cut
        // through the globe or skip the ocean at a concave country boundary.
        for (int n = 0; n < 3; ++n) {
            const auto& a = p[n]; const auto& b = p[(n + 1) % 3];
            const float cosang = (a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) /
                                 (1.0015f * 1.0015f);
            assert(cosang > std::cos(3.f * pi / 180.f) - 1e-4f);
        }

        const int c = static_cast<int>(std::round(geom.fill_xyz_rgb[i + 3] * 100));
        if (c == 93) {
            ++red;
            // Test shape: square with an interior hole.
            float lon = 0, lat = 0;
            for (const auto& a : p) {
                lon += std::atan2(a[2], a[0]) * 180 / pi / 3;
                lat += std::asin(a[1] / 1.0015f) * 180 / pi / 3;
            }
            assert(!(lon > 5.001f && lon < 14.999f && lat > 5.001f && lat < 14.999f));
            assert(lon >= -0.01 && lon <= 20.01 && lat >= -0.01 && lat <= 20.01);
        } else if (c == 45) {
            ++green;
            // Concave L-shaped polygon: the removed top-right area is ocean.
            float lon = 0, lat = 0;
            for (const auto& a : p) {
                lon += std::atan2(a[2], a[0]) * 180 / pi / 3;
                lat += std::asin(a[1] / 1.0015f) * 180 / pi / 3;
            }
            assert(!(lon > 35.001 && lat > 5.001));
        } else if (c == 90) {
            ++pink;
            // Polygon straddling 180/-180: no triangle may span the Earth.
            for (const auto& a : p)
                assert(std::abs(std::atan2(a[2], a[0]) * 180 / pi) >= 177.0);
        } else assert(false); // each feature retained its own MAPCOLOR7
    }
    assert(red > 0 && green > 0 && pink > 0);
}
} // namespace

int main() {
    // Test feature order permutations, a concave exterior, a hole, and the
    // antimeridian. Hole edges are also border strips but are never filled.
    const std::string json = R"JSON({
      "type":"FeatureCollection", "features":[
        {"type":"Feature", "properties":{"MAPCOLOR7":1,"NAME":"With a hole"},
         "geometry":{"type":"Polygon", "coordinates":[
           [[0,0],[20,0],[20,20],[0,20],[0,0]],
           [[5,5],[5,15],[15,15],[15,5],[5,5]]
         ]}},
        {"type":"Feature", "properties":{"NAME":"Concave","MAPCOLOR7":2},
         "geometry":{"type":"Polygon", "coordinates":[
           [[30,0],[50,0],[50,5],[35,5],[35,20],[30,20],[30,0]]
         ]}},
        {"type":"Feature", "geometry":{"coordinates":[
           [[[178,0],[-179,0],[-179,5],[178,5],[178,0]]]
         ],"type":"MultiPolygon"}, "properties":{"MAPCOLOR7":5}}
      ]
    })JSON";

    CountryGeometry geo;
    std::string error;
    assert(build_country_geometry(json, geo, &error));
    assert(geo.feature_count == 3);
    assert(geo.polygon_count == 3);
    assert(geo.border_strips.size() == 4);
    assert(geo.triangulation_failures == 0);
    for (const auto& r : geo.border_strips) {
        assert(r.count >= 4);
        const std::size_t first = static_cast<std::size_t>(r.first) * 3;
        const std::size_t last = static_cast<std::size_t>(r.first + r.count - 1) * 3;
        for (std::size_t k = 0; k < 3; ++k)
            assert(std::abs(geo.border_xyz[first + k] - geo.border_xyz[last + k]) < 1e-5f);
    }
    verify_triangles(geo);

    CountryGeometry invalid;
    assert(!build_country_geometry("{\"features\":[[[broken]]]}", invalid));
    assert(invalid.fill_xyz_rgb.empty());
}
