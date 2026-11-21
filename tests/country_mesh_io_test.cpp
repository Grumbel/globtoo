#include "country_geometry.hpp"
#include "country_mesh_io.hpp"
#include "country_lod.hpp"

#include <cassert>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    const std::string json = R"JSON({"type":"FeatureCollection","features":[
      {"type":"Feature","properties":{"NAME":"Local","NAME_EN":"English Name","MAPCOLOR7":3},
      "geometry":{"type":"Polygon","coordinates":[
        [[0,0],[4,0],[4,4],[0,4],[0,0]],
        [[1,1],[1,2],[2,2],[2,1],[1,1]]
      ]}}
    ]})JSON";
    CountryGeometry original;
    assert(build_country_geometry(json, original));
    assert(original.triangulation_failures == 0);
    assert(original.labels.size() == 1);
    assert(original.labels[0].name == "English Name");
    assert(original.labels[0].longitude >= 0.f && original.labels[0].longitude <= 4.f);
    assert(original.labels[0].latitude >= 0.f && original.labels[0].latitude <= 4.f);
    assert(original.border_lonlat.size() == original.border_xyz.size()/3*2);
    assert(original.fill_lonlat.size() == original.fill_xyz_rgb.size()/6*2);
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
    assert(restored.border_lonlat == original.border_lonlat);
    assert(restored.fill_lonlat == original.fill_lonlat);
    assert(restored.labels.size() == original.labels.size());
    assert(restored.labels[0].name == "English Name");
    assert(restored.labels[0].longitude == original.labels[0].longitude);
    assert(restored.labels[0].latitude == original.labels[0].latitude);
    assert(restored.border_strips.size() == original.border_strips.size());
    for (std::size_t i = 0; i < restored.border_strips.size(); ++i) {
        assert(restored.border_strips[i].first == original.border_strips[i].first);
        assert(restored.border_strips[i].count == original.border_strips[i].count);
    }
    // Legacy v1 meshes are readable, although they have no country labels and
    // cannot retain the longitude unwrap that v2 adds for 2D projections.
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        auto put = [&](std::uint32_t value) {
            for (int i=0;i<4;i++) f.put(static_cast<char>(value>>(8*i)));
        };
        f.write("GLOBGM01",8);
        for (std::size_t n : {original.border_xyz.size(), original.border_strips.size(),
             original.fill_xyz_rgb.size(),original.feature_count,original.polygon_count,
             original.triangulation_failures}) put(static_cast<std::uint32_t>(n));
        for (float x:original.border_xyz) put(std::bit_cast<std::uint32_t>(x));
        for (auto strip:original.border_strips) { put(strip.first); put(strip.count); }
        for (float x:original.fill_xyz_rgb) put(std::bit_cast<std::uint32_t>(x));
    }
    assert(read_country_mesh(path.string(), restored, &error));
    assert(restored.labels.empty());
    assert(restored.border_lonlat.size()==original.border_lonlat.size());
    assert(restored.fill_lonlat.size()==original.fill_lonlat.size());

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
