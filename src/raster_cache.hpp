// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "tiled_raster.hpp"
#include "raster_legend.hpp"
#include "mesh.hpp"
#include "shader.hpp"
#include "texture.hpp"
#include "camera.hpp"
#include "map_view.hpp"
#include <cstdint>
#include <string>
#include <vector>

struct RasterGpuCell {
    Mesh mesh{};
    Texture image{};
    std::uint64_t recent = 0;
    bool failed = false;
};
struct RasterCache {
    RasterTiles layout{};
    std::string root;
    Texture preview{};
    RasterLegend legend{};
    std::vector<RasterGpuCell> cells;
    std::uint64_t frame = 0;
    int loaded_count = 0;
    bool valid() const { return preview.id && !cells.empty(); }
};

bool raster_cache_init(RasterCache& r, const std::string& manifest_path);
void raster_cache_shutdown(RasterCache& r);
// Read at most three files per frame; use the preview while tiles stream from
// the installed disk. Returns true only when ALL visible tiles are ready.
bool raster_cache_ready(RasterCache& r,const Camera& cam,const MapView& map,
                        float aspect,std::vector<int>& visible);
bool raster_cache_draw(RasterCache& r, const Camera& cam,const MapView& map,
                       float aspect, const Shader& shader);
