// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_cache.hpp"
#include "geocoordinates.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace {
constexpr int max_cached=24;
constexpr int max_drawn=24;
constexpr int loads_per_frame=3;
constexpr double pi=geo::pi;

double angular_distance(const std::array<double,3>& a,const std::array<double,3>& b) {
    const double dot=a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    return std::acos(std::clamp(dot,-1.,1.));
}

// Conservative geographic selection: only attempt detailed imagery when the
// viewport covers a modest angular region. Every tile meeting the bound is
// drawn, not just those with their midpoint on screen (important at high zoom).
bool visible_tile(const RasterTileRegion& tile,const std::array<double,3>& focus,
                  double viewport_radius) {
    const auto center=geo::lonlat_to_xyz(tile.lon,tile.lat);
    return angular_distance(center,focus)<=viewport_radius+tile.angular_radius;
}
}

bool raster_cache_init(RasterCache& r,const std::string& path) {
    raster_cache_shutdown(r);
    std::string error;
    if (!std::filesystem::is_regular_file(path)) return false;
    if (!raster_tiles_read(path,r.layout,error)) {
        fprintf(stderr,"Invalid raster tile manifest: %s\n",error.c_str());
        return false;
    }
    r.root=std::filesystem::path(path).parent_path().string();
    r.preview=load_texture((std::filesystem::path(r.root)/"preview.jpg").string().c_str(),false);
    if (!r.preview.id)
        r.preview=load_texture((std::filesystem::path(r.root)/"preview-small.jpg").string().c_str(),false);
    if (!r.preview.id) { raster_cache_shutdown(r); return false; }
    std::string legend_error;
    // Optional for older, purely visual relief tiles. Required for measured
    // elevation layers, where users must see the physical units and source.
    raster_legend_read((std::filesystem::path(r.root)/"legend.txt").string(),
                       r.legend, legend_error);
    r.cells.resize(static_cast<size_t>(r.layout.tile_count()));
    fprintf(stderr,"Raster: %dx%d source, %d tiles (lazy GLES2 upload)\n",
            r.layout.width,r.layout.height,r.layout.tile_count());
    return true;
}
void raster_cache_shutdown(RasterCache& r) {
    for (auto& t : r.cells) { destroy_mesh(t.mesh); destroy_texture(t.image); }
    destroy_texture(r.preview);
    r=RasterCache{};
}

bool raster_cache_ready(RasterCache& r,const Camera& cam,const MapView& map,
                        float aspect,std::vector<int>& visible) {
    visible.clear();
    if (!r.valid()) return false;
    const bool sphere=map.globe_to_flat<.001f;
    const bool flat=map.globe_to_flat>.999f;
    if (!sphere && !flat) return false; // Morphs use seamless world preview.
    if (sphere && cam.distance>1.44f) return false;
    if (flat && map.zoom<15.f) return false;
    std::array<double,3> focus{};
    double radius=pi;
    if (sphere) {
        const auto v=camera_view_to_world(cam,{0,0,1});
        focus={v.x,v.y,v.z};
        // Limbs are occluded by the globe. The visible angular cap shrinks
        // sharply at close range; include a small safety margin for pixels.
        radius=std::acos(std::clamp(1./double(cam.distance),0.,1.))+.10;
    } else {
        const auto center=map_sheet_center(map);
        if (!center) return false;
        focus=geo::lonlat_to_xyz((*center)[0]*geo::to_deg,(*center)[1]*geo::to_deg);
        radius=std::min(pi,4.*std::tan(.35)*map.map_distance /
                        std::max(1.f,map.zoom) * std::hypot(1.f,aspect));
    }
    for (int i=0;i<r.layout.tile_count();++i) {
        const int col=i%r.layout.columns,row=i/r.layout.columns;
        if (visible_tile(raster_tile_region(r.layout,col,row),focus,radius))
            visible.push_back(i);
    }
    if (visible.empty() || visible.size()>max_drawn) return false;
    ++r.frame;
    for (int index:visible) r.cells[static_cast<size_t>(index)].recent=r.frame;
    int pending=0, budget=loads_per_frame;
    for (int index : visible) {
        auto& cell=r.cells[static_cast<size_t>(index)];
        if (cell.image.id) continue;
        if (cell.failed) return false;
        ++pending;
        if (!budget) continue;
        --budget;
        // Always evict offscreen tiles before uploading to avoid 100s of MB
        // accumulating in GPU memory as the user moves around the world.
        if (r.loaded_count>=max_cached) {
            size_t oldest=0;
            std::uint64_t last=r.frame;
            bool found=false;
            for (size_t j=0;j<r.cells.size();++j) {
                const auto& candidate=r.cells[j];
                if (candidate.image.id && candidate.recent<last) {
                    oldest=j; last=candidate.recent; found=true;
                }
            }
            if (!found) break;
            auto& victim=r.cells[oldest];
            destroy_mesh(victim.mesh);
            destroy_texture(victim.image);
            --r.loaded_count;
        }
        const auto path=std::filesystem::path(r.root)/
            raster_tile_filename(index%r.layout.columns,index/r.layout.columns);
        cell.image=load_texture(path.string().c_str(),false);
        if (!cell.image.id) {
            // Leave preview displayed; avoid infinite repeated disk/GPU attempts.
            fprintf(stderr,"Raster tile unavailable: %s\n",path.string().c_str());
            cell.failed=true;
            return false;
        }
        ++r.loaded_count;
        const auto reg=raster_tile_region(r.layout,index%r.layout.columns,index/r.layout.columns);
        cell.mesh=create_sphere_region(12,12,reg.u0,reg.v0,reg.u1,reg.v1,
                                      reg.tex_u0,reg.tex_v0,reg.tex_u1,reg.tex_v1);
    }
    return pending==0; // One stable frame with all required tiles loaded.
}

bool raster_cache_draw(RasterCache& r,const Camera& cam,const MapView& map,
                       float aspect,const Shader& shader) {
    std::vector<int> visible;
    if (!raster_cache_ready(r,cam,map,aspect,visible)) return false;
    const bool copies=map.globe_to_flat>.999f && scene_map_seam_visible(map,aspect);
    for (int copy=(copies?-1:0);copy<=(copies?1:0);++copy) {
        scene_uniforms(shader,cam,aspect,map,static_cast<float>(copy));
        for (int index:visible) {
            auto& cell=r.cells[static_cast<size_t>(index)];
            bind_texture(cell.image,0);
            draw_mesh(cell.mesh,map.globe_to_flat>.001f);
        }
    }
    return true;
}
