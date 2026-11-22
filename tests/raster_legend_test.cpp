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
    const std::string density =
        "GLOBTOO_RASTER_LEGEND 2\nPOPULATION DENSITY 2020\nPEOPLE / KM2\n"
        "GPWV4 R11 (2020, 30 ARCSEC)\nLOG1P\n"
        "3\n0 240 239 226\n100 82 185 131\n20000 56 26 73\n";
    assert(raster_legend_parse(density,data,error) && data.valid());
    assert(data.logarithmic && data.units=="PEOPLE / KM2");
    assert(!raster_legend_parse(density.substr(0,density.find("LOG1P"))+
                               "BAD_SCALE\n3\n0 0 0 0\n1 1 1 1\n2 2 2 2\n",data,error));
    assert(!raster_legend_parse("GLOBTOO_RASTER_LEGEND 2\nA\nB\nC\nLOG1P\n"
                               "2\n-1 0 0 0\n100 1 2 3\n",data,error));
    if (argc>1) {
        assert(raster_legend_read(argv[1],data,error));
        assert(data.valid());
        if (data.units=="METRES") {
            assert(!data.logarithmic);
            assert(data.stops.front().value < 0 && data.stops.back().value > 0);
        } else {
            assert(data.units=="PEOPLE / KM2" && data.logarithmic);
            assert(data.stops.front().value==0 && data.stops.back().value>1000);
        }
    }
    std::cout<<"raster numeric legend: PASS\n";
}
