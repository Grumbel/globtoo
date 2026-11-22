// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tiled_raster.hpp"
#include <cassert>
#include <cmath>
#include <iostream>

int main(int argc, char** argv) {
    RasterTiles t{};
    std::string error;
    assert(raster_tiles_parse("GLOBTOO_RASTER_TILES 1\n21600 10800 1024 4 22 11\n",t,error));
    assert(t.tile_count()==242 && t.content_size()==1016);
    const auto first=raster_tile_region(t,0,0), last=raster_tile_region(t,21,10);
    assert(first.u0==0 && first.v0==0);
    assert(last.u1==1 && last.v1==1);
    assert(first.tex_u0==4./1024 && first.tex_u1==1020./1024);
    assert(last.tex_u1 < 1 && last.tex_v1 < 1);
    assert(std::abs(first.lon+180.-180.*(first.u1-first.u0))<1e-5);
    assert(raster_tile_filename(2,4)=="tile-004-002.jpg");
    // Adjacent GPU meshes must share their exact geographic boundary.
    for (int row=0;row<t.rows;++row) {
        for (int col=0;col<t.columns;++col) {
            const auto a=raster_tile_region(t,col,row);
            if (col+1<t.columns) {
                const auto b=raster_tile_region(t,col+1,row);
                assert(a.u1==b.u0 && a.v0==b.v0 && a.v1==b.v1);
            }
            if (row+1<t.rows) {
                const auto b=raster_tile_region(t,col,row+1);
                assert(a.v1==b.v0 && a.u0==b.u0 && a.u1==b.u1);
            }
        }
    }
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 1\n21600 10800 1024 4 22 12",t,error));
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 1\n21600 10800 1000 4 22 11",t,error));
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 2\n21600 10800 1024 4 22 11",t,error));
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 1\n21600 10800 1024 4 22 11 xyz",t,error));
    if (argc>1) {
        assert(raster_tiles_read(argv[1],t,error));
        assert(t.tile_count()>0);
    }
    std::cout<<"raster tile manifest tests passed\n";
}
