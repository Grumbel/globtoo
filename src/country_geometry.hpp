// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// CPU-only geometry builder. All coordinates in the same Y-up frame as the
// globe mesh; triangles are CCW as seen from outside the Earth.
struct CountryGeometry {
    struct Strip { int first = 0; int count = 0; };
    struct Label { std::string name; float longitude = 0, latitude = 0; };
    // Separate lon/lat attributes preserve unwrapped antimeridian geometry.
    std::vector<float> border_lonlat;
    std::vector<float> fill_lonlat;
    std::vector<Label> labels;
    std::vector<float> border_xyz;
    std::vector<Strip> border_strips;
    std::vector<float> fill_xyz_rgb;
    std::size_t feature_count = 0;
    std::size_t polygon_count = 0;
    std::size_t triangulation_failures = 0;
};

// Natural Earth GeoJSON FeatureCollection, including Polygon, MultiPolygon,
// interior holes and antimeridian rings. Returns false on malformed JSON.
bool build_country_geometry(std::string_view geojson, CountryGeometry& out,
                            std::string* error = nullptr);
