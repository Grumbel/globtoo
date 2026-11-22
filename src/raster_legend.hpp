// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct RasterLegendStop {
    int value = 0;
    std::uint8_t red=0, green=0, blue=0;
};
struct RasterLegend {
    std::string title;
    std::string units;
    std::string attribution;
    std::vector<RasterLegendStop> stops;
    bool valid() const { return stops.size()>=2; }
};

bool raster_legend_parse(std::string_view data, RasterLegend& out, std::string& error);
bool raster_legend_read(const std::string& filename, RasterLegend& out, std::string& error);
