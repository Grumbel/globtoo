#include "country_geometry.hpp"
#include "country_mesh_io.hpp"
#include "country_lod.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const std::string json = R"JSON({"type":"FeatureCollection","features":[
      {"type":"Feature","properties":{"NAME":"Complex","MAPCOLOR7":3},
      "geometry":{"type":"Polygon","coordinates":[
        [[0,0],[4,0],[4,4],[0,4],[0,0]],
        [[1,1],[1,2],[2,2],[2,1],[1,1]]
      ]}}
    ]})JSON";
    CountryGeometry original;
    assert(build_country_geometry(json, original));
    assert(original.triangulation_failures == 0);
    assert(!original.fill_xyz_rgb.empty());
    const auto path = std::filesystem::temp_directory_path() / "globtoo-test-countries.gmesh";
    std::string error;
    assert(write_country_mesh(path.string(), original, &error));
    CountryGeometry restored;
    assert(read_country_mesh(path.string(), restored, &error));
    assert(restored.feature_count == original.feature_count);
    assert(restored.polygon_count == original.polygon_count);
    assert(restored.triangulation_failures == original.triangulation_failures);
    assert(restored.fill_xyz_rgb == original.fill_xyz_rgb);
    assert(restored.border_xyz == original.border_xyz);
    assert(restored.border_strips.size() == original.border_strips.size());
    for (std::size_t i = 0; i < restored.border_strips.size(); ++i) {
        assert(restored.border_strips[i].first == original.border_strips[i].first);
        assert(restored.border_strips[i].count == original.border_strips[i].count);
    }
    // An incorrectly sized or truncated asset must not allocate huge buffers.
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f << "GLOBGM01" << "BROKEN";
    }
    assert(!read_country_mesh(path.string(), restored, &error));
    assert(restored.fill_xyz_rgb.empty());
    std::filesystem::remove(path);

    assert(!country_use_high_detail(3.2f, false, true, true));
    assert(!country_use_high_detail(2.f, false, true, true));
    assert(country_use_high_detail(1.8f, false, true, true));
    assert(country_use_high_detail(2.0f, true, true, true));
    assert(!country_use_high_detail(2.2f, true, true, true));
    assert(!country_use_high_detail(1.2f, false, false, true));
    assert(country_use_high_detail(3.2f, false, true, false));
}
