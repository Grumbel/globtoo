// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

// All tile images are power-of-two JPEGs, with a duplicated/cyclic gutter.
// Each cell covers a geographic UV rectangle; JPEG image UVs exclude gutters.
// The manifest is deliberately small, text-based, and versioned.
// Version 2 adds a pyramid: level 0 is the full-resolution grid, every
// coarser level halves the height (rounding up) and keeps an exact 2:1 width.
// All levels share tile size and gutter. Must match tools/raster_manifest.py.
struct RasterTiles {
    static constexpr int max_levels = 8;
    static constexpr int max_tiles = 8192;
    // Widest accepted source: the 109164-pixel LROC WAC 100 m Moon mosaic.
    static constexpr int max_width = 131072;
    int width = 0, height = 0;
    int tile_size = 0, gutter = 0;
    int columns = 0, rows = 0;
    int levels = 1;
    int content_size() const { return tile_size - 2 * gutter; }
    int level_height(int level) const;
    int level_width(int level) const { return 2 * level_height(level); }
    int level_columns(int level) const;
    int level_rows(int level) const;
    // Cells of all levels are numbered consecutively, finest level first.
    int level_offset(int level) const;
    int tile_count() const { return level_offset(levels); }
};

struct RasterTileRegion {
    double u0=0, v0=0, u1=0, v1=0;
    double tex_u0=0, tex_v0=0, tex_u1=0, tex_v1=0;
    double lon=0, lat=0; // tile center in degrees
    double angular_radius=0; // conservative spherical radius, radians
};

bool raster_tiles_parse(std::string_view data, RasterTiles& out, std::string& error);
bool raster_tiles_read(const std::string& filename, RasterTiles& out, std::string& error);
RasterTileRegion raster_tile_region(const RasterTiles& tiles, int column, int row,
                                    int level = 0);
std::string raster_tile_filename(int column, int row, int level = 0);
