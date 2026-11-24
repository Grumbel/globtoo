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
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct RasterGpuCell {
    // Static geometry, filled in at init.
    RasterTileRegion region{};
    std::array<double,3> center{};
    int level = 0, column = 0, row = 0;
    // Streaming state.
    Mesh mesh{};
    Texture image{};
    std::uint64_t recent = 0; // last frame the LOD selection wanted it
    bool failed = false;
};
struct RasterLoader; // background JPEG decoder, private to raster_cache.cpp
struct RasterCache {
    RasterTiles layout{};
    std::string root;
    Texture preview{};
    RasterLegend legend{};
    std::vector<RasterGpuCell> cells; // all pyramid levels, finest first
    std::uint64_t frame = 0;
    int loaded_count = 0;
    std::shared_ptr<RasterLoader> loader; // started on first request
    bool valid() const { return preview.id && !cells.empty(); }
};

// What the tile selection needs to know about the current frame.
struct RasterView {
    float aspect = 1;
    int viewport_height = 1; // pixels
    // Width of the whole-world texture drawn underneath the tiles. Tiles are
    // only requested where they are sharper than this base texture.
    int base_width = 0;
};

bool raster_cache_init(RasterCache& r, const std::string& manifest_path);
void raster_cache_shutdown(RasterCache& r);
// Select tiles by on-screen texel density, stream them in the background and
// draw every loaded visible tile, finest level first. Callers composite with
// the stencil buffer so each pixel takes the finest loaded tile and the base
// texture fills the rest (see globe_draw). Only the fully round globe or the
// fully flat map use tiles; returns the number of tiles drawn.
int raster_cache_draw(RasterCache& r, const Camera& cam, const MapView& map,
                      const RasterView& view, const Shader& shader);
