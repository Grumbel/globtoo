// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

// All tile images are power-of-two JPEGs, with a duplicated/cyclic gutter.
// Each cell covers a geographic UV rectangle; JPEG image UVs exclude gutters.
// The manifest is deliberately small, text-based, and versioned.
struct RasterTiles {
    int width = 0, height = 0;
    int tile_size = 0, gutter = 0;
    int columns = 0, rows = 0;
    int content_size() const { return tile_size - 2 * gutter; }
    int tile_count() const { return columns * rows; }
};

struct RasterTileRegion {
    double u0=0, v0=0, u1=0, v1=0;
    double tex_u0=0, tex_v0=0, tex_u1=0, tex_v1=0;
    double lon=0, lat=0; // tile center in degrees
    double angular_radius=0; // conservative spherical radius, radians
};

bool raster_tiles_parse(std::string_view data, RasterTiles& out, std::string& error);
bool raster_tiles_read(const std::string& filename, RasterTiles& out, std::string& error);
RasterTileRegion raster_tile_region(const RasterTiles& tiles, int column, int row);
std::string raster_tile_filename(int column, int row);
