// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct GeographicPlace {
    float longitude=0, latitude=0;
    std::uint32_t population=0;
    std::uint8_t scale_rank=10;
    bool capital=false;
    std::string name, alternate_name;
};
struct GeographicRiverSegment {
    float a[3]{}, b[3]{}; // unit vectors in Globtoo's right-handed geographic frame
    std::uint8_t scale_rank=10;
};
struct GeographicFeatures {
    std::vector<GeographicPlace> places;
    std::vector<GeographicRiverSegment> rivers;
};

bool read_geographic_features(const std::string& filename, GeographicFeatures& output,
                              std::string* error=nullptr);
// Visibility tiers: capitals remain visible at a distance, non-capital
// cities/river tributaries become more detailed as a region is zoomed in.
unsigned geographic_detail_rank(float camera_distance, float map_zoom, bool flat);
bool geographic_city_visible(const GeographicPlace& place, unsigned rank,
                             bool show_capitals, bool show_cities);
