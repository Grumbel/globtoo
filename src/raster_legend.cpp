// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_legend.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>

namespace {
// Labels in the compact renderer must remain within its supported ASCII font.
bool safe_line(const std::string& s) {
    return !s.empty() && s.size()<=90 &&
           std::all_of(s.begin(),s.end(),[](unsigned char c) {return c>=32 && c<=126;});
}
}

bool raster_legend_parse(std::string_view data, RasterLegend& out, std::string& error) {
    out={};
    if (data.size()>4096) { error="legend too large"; return false; }
    std::istringstream in{std::string(data)};
    std::string header;
    RasterLegend parsed;
    int count=0;
    if (!std::getline(in,header) || header!="GLOBTOO_RASTER_LEGEND 1" ||
        !std::getline(in,parsed.title) || !std::getline(in,parsed.units) ||
        !std::getline(in,parsed.attribution) || !(in >> count) ||
        !safe_line(parsed.title) || !safe_line(parsed.units) ||
        !safe_line(parsed.attribution) || count<2 || count>32) {
        error="invalid raster legend header";
        return false;
    }
    for (int i=0;i<count;i++) {
        int value=0,r=0,g=0,b=0;
        if (!(in >> value >> r >> g >> b) ||
            value < -32767 || value > 32767 ||
            r<0 || r>255 || g<0 || g>255 || b<0 || b>255 ||
            (!parsed.stops.empty() && value<=parsed.stops.back().value)) {
            error="invalid raster legend color stop"; return false;
        }
        parsed.stops.push_back({value,static_cast<std::uint8_t>(r),
            static_cast<std::uint8_t>(g),static_cast<std::uint8_t>(b)});
    }
    std::string trailing;
    if (in >> trailing) { error="unexpected trailing legend data"; return false; }
    out=std::move(parsed);
    error.clear();
    return true;
}

bool raster_legend_read(const std::string& filename, RasterLegend& out, std::string& error) {
    std::ifstream file(filename);
    if (!file) { error="cannot open raster legend: "+filename; return false; }
    std::string data((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    return raster_legend_parse(data,out,error);
}
