// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_legend.hpp"
#include <cassert>
#include <string>
#include <iostream>

int main(int argc,char** argv) {
    const std::string example =
        "GLOBTOO_RASTER_LEGEND 1\nELEVATION AND BATHYMETRY\nMETRES\nNOAA ETOPO1 (2009)\n"
        "3\n-11000 4 12 22\n0 50 120 80\n9000 255 255 255\n";
    RasterLegend data;
    std::string error;
    assert(raster_legend_parse(example,data,error));
    assert(data.valid() && data.stops.size()==3);
    assert(data.stops.front().value==-11000 && data.stops[1].value==0);
    assert(data.attribution=="NOAA ETOPO1 (2009)");
    assert(!raster_legend_parse(example+"bad\n",data,error));
    assert(!data.valid());
    const std::string unordered =
        "GLOBTOO_RASTER_LEGEND 1\nA\nm\nB\n2\n0 0 0 0\n0 20 10 20\n";
    assert(!raster_legend_parse(unordered,data,error));
    const std::string invalid =
        "GLOBTOO_RASTER_LEGEND 1\nA\nm\nB\n2\n0 500 0 0\n1 20 10 20\n";
    assert(!raster_legend_parse(invalid,data,error));
    if (argc>1) {
        assert(raster_legend_read(argv[1],data,error));
        assert(data.valid() && data.units=="METRES");
        assert(data.stops.front().value < 0 && data.stops.back().value > 0);
    }
    std::cout<<"raster numeric legend: PASS\n";
}
