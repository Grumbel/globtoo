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
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 2\n21600 10800 1024 4 22 11 1",t,error));
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 1\n21600 10800 1024 4 22 11 4",t,error));
    // Pyramid: 21600 -> 10800 -> 5400 -> 2700 wide, matching the Python tiler.
    assert(raster_tiles_parse("GLOBTOO_RASTER_TILES 2\n21600 10800 1024 4 22 11 4\n",t,error));
    assert(t.levels==4 && t.level_width(3)==2700 && t.level_height(3)==1350);
    assert(t.level_columns(1)==11 && t.level_rows(1)==6);
    assert(t.level_columns(3)==3 && t.level_rows(3)==2);
    assert(t.level_offset(1)==242 && t.tile_count()==242+66+18+6);
    assert(raster_tile_filename(8,4,1)=="tile-l1-004-008.jpg");
    for (int level=0;level<t.levels;++level) {
        const auto end=raster_tile_region(t,t.level_columns(level)-1,
                                          t.level_rows(level)-1,level);
        assert(end.u1==1 && end.v1==1 && end.u0<1 && end.v0<1);
    }
    assert(raster_tile_region(t,3,0,3).angular_radius==0); // out of range
    // Odd heights round up: 4097 rows -> 2049 -> 1025.
    assert(raster_tiles_parse("GLOBTOO_RASTER_TILES 2\n8194 4097 1024 4 9 5 3",t,error));
    assert(t.level_height(1)==2049 && t.level_width(2)==2050);
    assert(!raster_tiles_parse("GLOBTOO_RASTER_TILES 1\n21600 10800 1024 4 22 11 xyz",t,error));
    if (argc>1) {
        assert(raster_tiles_read(argv[1],t,error));
        assert(t.tile_count()>0);
    }
    std::cout<<"raster tile manifest tests passed\n";
}
