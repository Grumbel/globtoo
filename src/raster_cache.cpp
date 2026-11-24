// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "raster_cache.hpp"
#include "geocoordinates.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <thread>

namespace {
// 1024² RGBA tiles with mipmaps take ~5.3 MiB of GPU memory each.
constexpr int max_cached=32;
// Decoding runs on the loader thread; only the GL upload (incl. mipmap
// generation) happens on the render thread, so keep that per-frame cost low.
constexpr int uploads_per_frame=2;
// Decoded but not yet uploaded tiles (RGBA, 4 MiB each at 1024²).
constexpr size_t max_decoded=4;
constexpr double pi=geo::pi;

double angular_distance(const std::array<double,3>& a,const std::array<double,3>& b) {
    const double dot=a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
    return std::acos(std::clamp(dot,-1.,1.));
}

struct Request {
    int index=0;
    std::string path;
};
struct Decoded {
    int index=0;
    ImageRGBA image;
    bool ok=false;
};
} // namespace

struct RasterLoader {
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<Request> queue; // highest priority at the back
    std::vector<Decoded> done;
    int busy=-1;
    bool stop=false;
    const int tile_size;
    std::thread thread; // last member: everything above exists before it runs

    explicit RasterLoader(int size) : tile_size(size), thread([this] { run(); }) {}
    ~RasterLoader() {
        { std::lock_guard<std::mutex> lock(mutex); stop=true; }
        wake.notify_all();
        thread.join();
    }
    void run() {
        std::unique_lock<std::mutex> lock(mutex);
        for (;;) {
            wake.wait(lock,[this] {
                return stop || (!queue.empty() && done.size()<max_decoded);
            });
            if (stop) return;
            Request request=std::move(queue.back());
            queue.pop_back();
            busy=request.index;
            lock.unlock();
            Decoded result{request.index,{},false};
            result.ok=decode_image_rgba(request.path.c_str(),tile_size,tile_size,
                                        result.image);
            lock.lock();
            busy=-1;
            done.push_back(std::move(result));
        }
    }
};

namespace {
// Pyramid level that is just sharp enough for `pixels_per_radian` on screen;
// t.levels means the whole-world base texture already suffices.
int needed_level(const RasterTiles& t,int base_width,double pixels_per_radian) {
    const double texels=pixels_per_radian*2*pi; // across the full 360 degrees
    if (base_width>=texels) return t.levels;
    for (int level=t.levels-1;level>0;--level)
        if (t.level_width(level)>=texels) return level;
    return 0;
}

// Tessellate proportional to angular size, so coarse-level tiles spanning
// tens of degrees keep a smooth silhouette (~1.5 degrees per segment).
int segments(double span_degrees) {
    return std::clamp(static_cast<int>(std::ceil(span_degrees/1.5)),4,128);
}

bool make_room(RasterCache& r) {
    if (r.loaded_count<max_cached) return true;
    RasterGpuCell* victim=nullptr;
    for (auto& cell : r.cells) {
        // Only evict tiles the current frame does not want.
        if (cell.image.id && cell.recent<r.frame &&
            (!victim || cell.recent<victim->recent)) victim=&cell;
    }
    if (!victim) return false;
    destroy_mesh(victim->mesh);
    destroy_texture(victim->image);
    --r.loaded_count;
    return true;
}

void upload(RasterCache& r,RasterGpuCell& cell,const ImageRGBA& image) {
    cell.image=upload_texture_rgba(image,false);
    if (!cell.image.id) { cell.failed=true; return; }
    const auto& g=cell.region;
    cell.mesh=create_sphere_region(segments(360.*(g.u1-g.u0)),segments(180.*(g.v1-g.v0)),
                                   g.u0,g.v0,g.u1,g.v1,
                                   g.tex_u0,g.tex_v0,g.tex_u1,g.tex_v1);
    ++r.loaded_count;
}

// The part of the surface that can be on screen, as a spherical cap around
// `focus`, plus the screen density in pixels per radian of surface.
struct ViewCap {
    std::array<double,3> focus{};
    double radius=0;
    bool sphere=true;
    double distance=0;   // sphere: camera distance from the centre
    double density=0;    // sphere: pixels/radian at unit distance; flat: pixels/radian
    double pixels_per_radian(double gamma) const {
        if (!sphere) return density;
        // Distance from the camera to a surface point gamma radians from
        // the sub-camera point. Ignores foreshortening (conservative).
        const double s=std::sqrt(std::max(1e-12,distance*distance+1-
                                          2*distance*std::cos(gamma)));
        return density/s;
    }
};

bool view_cap(const Camera& cam,const MapView& map,const RasterView& view,ViewCap& out) {
    const double tan_half_fov=std::tan(.35); // Camera::proj_matrix uses fovy .7
    const double half_height=.5*std::max(1,view.viewport_height);
    const double half_diagonal=std::atan(tan_half_fov*std::hypot(1.,double(view.aspect)));
    if (map.globe_to_flat<.001f) {
        const auto v=camera_view_to_world(cam,{0,0,1});
        const double d=std::max(1.0001,double(cam.distance));
        out.focus={v.x,v.y,v.z};
        out.sphere=true;
        out.distance=d;
        out.density=half_height/tan_half_fov;
        // The globe limb bounds the visible cap; up close the viewport's
        // corner rays hit the surface first and bound it much tighter.
        const double horizon=std::acos(1./d);
        const double corner=d*std::sin(half_diagonal);
        const double frustum=corner<1 ? std::asin(corner)-half_diagonal : horizon;
        out.radius=std::min(horizon,frustum)+.02;
        return true;
    }
    if (map.globe_to_flat>.999f) {
        const auto center=map_sheet_center(map);
        if (!center) return false;
        out.focus=geo::lonlat_to_xyz((*center)[0]*geo::to_deg,(*center)[1]*geo::to_deg);
        out.sphere=false;
        // One map unit is one radian of longitude at zoom 1.
        const double units=tan_half_fov*map.map_distance;
        out.density=half_height/units*map.zoom;
        // Generous: equal-area and rolled projections stretch the sheet.
        out.radius=std::min(pi,2.*units*std::hypot(1.,double(view.aspect))/
                            std::max(.6f,map.zoom));
        return true;
    }
    return false; // Morphs use the seamless whole-world texture only.
}

struct Candidate {
    int index=0;
    double gamma=0;   // centre distance from the focus, radians
    int finest=0;     // level needed at the tile's nearest visible point
    int coarsest=0;   // level needed at its farthest visible point
};
} // namespace

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
    for (int level=0;level<r.layout.levels;++level) {
        const int columns=r.layout.level_columns(level);
        for (int row=0;row<r.layout.level_rows(level);++row) {
            for (int column=0;column<columns;++column) {
                auto& cell=r.cells[static_cast<size_t>(
                    r.layout.level_offset(level)+row*columns+column)];
                cell.level=level; cell.column=column; cell.row=row;
                cell.region=raster_tile_region(r.layout,column,row,level);
                cell.center=geo::lonlat_to_xyz(cell.region.lon,cell.region.lat);
            }
        }
    }
    fprintf(stderr,"Raster: %dx%d source, %d levels, %d tiles (streamed)\n",
            r.layout.width,r.layout.height,r.layout.levels,r.layout.tile_count());
    return true;
}

void raster_cache_shutdown(RasterCache& r) {
    r.loader.reset(); // joins the decoder before GL objects go away
    for (auto& t : r.cells) { destroy_mesh(t.mesh); destroy_texture(t.image); }
    destroy_texture(r.preview);
    r=RasterCache{};
}

int raster_cache_draw(RasterCache& r,const Camera& cam,const MapView& map,
                      const RasterView& view,const Shader& shader) {
    if (!r.valid()) return 0;
    ViewCap cap{};
    if (!view_cap(cam,map,view,cap)) return 0;
    const auto& layout=r.layout;

    // 1. Visible tiles with the level range their visible part needs.
    std::vector<Candidate> visible;
    for (size_t i=0;i<r.cells.size();++i) {
        const auto& cell=r.cells[i];
        const double gamma=angular_distance(cell.center,cap.focus);
        const double radius=cell.region.angular_radius;
        if (gamma>cap.radius+radius) continue;
        const double nearest=std::max(0.,gamma-radius);
        const double farthest=std::min(cap.radius,gamma+radius);
        visible.push_back({static_cast<int>(i),gamma,
            needed_level(layout,view.base_width,cap.pixels_per_radian(nearest)),
            needed_level(layout,view.base_width,cap.pixels_per_radian(farthest))});
    }

    // 2. A tile is wanted if some visible point of it needs exactly its
    // level, which covers every pixel with a sharp-enough level. Coarsen
    // the whole selection when that would exceed the cache.
    std::vector<int> wanted;
    for (int bias=0;bias<=layout.levels;++bias) {
        wanted.clear();
        for (const auto& c : visible) {
            const int level=r.cells[static_cast<size_t>(c.index)].level;
            if (std::min(layout.levels,c.finest+bias)<=level &&
                level<=std::min(layout.levels,c.coarsest+bias))
                wanted.push_back(c.index);
        }
        if (wanted.size()<=static_cast<size_t>(max_cached)) break;
    }
    ++r.frame;
    for (int index : wanted) r.cells[static_cast<size_t>(index)].recent=r.frame;

    // 3. Upload finished decodes and hand the decoder its new queue.
    std::vector<Decoded> ready;
    int busy=-1;
    if (r.loader) {
        std::lock_guard<std::mutex> lock(r.loader->mutex);
        ready.swap(r.loader->done);
        busy=r.loader->busy;
    }
    std::vector<Decoded> keep;
    int uploads=0;
    for (auto& decoded : ready) {
        auto& cell=r.cells[static_cast<size_t>(decoded.index)];
        if (!decoded.ok) {
            // Keep the coarser fallback; never retry a broken file.
            fprintf(stderr,"Raster tile unavailable: %s/%s\n",r.root.c_str(),
                    raster_tile_filename(cell.column,cell.row,cell.level).c_str());
            cell.failed=true;
        } else if (cell.recent!=r.frame || cell.image.id) {
            // No longer wanted; drop it.
        } else if (uploads>=uploads_per_frame) {
            keep.push_back(std::move(decoded));
        } else if (make_room(r)) {
            upload(r,cell,decoded.image);
            ++uploads;
        }
    }
    std::vector<std::pair<double,int>> order;
    for (int index : wanted) {
        const auto& cell=r.cells[static_cast<size_t>(index)];
        if (cell.image.id || cell.failed || index==busy ||
            std::any_of(keep.begin(),keep.end(),
                        [&](const Decoded& d) { return d.index==index; }))
            continue;
        order.emplace_back(angular_distance(cell.center,cap.focus),index);
    }
    if (!order.empty() && !r.loader)
        r.loader=std::make_shared<RasterLoader>(layout.tile_size);
    if (r.loader) {
        // Nearest to the view centre at the back, where the decoder pops.
        std::sort(order.begin(),order.end(),std::greater<>());
        std::vector<Request> queue;
        queue.reserve(order.size());
        for (const auto& entry : order) {
            const auto& cell=r.cells[static_cast<size_t>(entry.second)];
            queue.push_back({entry.second,(std::filesystem::path(r.root)/
                raster_tile_filename(cell.column,cell.row,cell.level)).string()});
        }
        {
            std::lock_guard<std::mutex> lock(r.loader->mutex);
            // The decoder may have taken a tile from the old queue meanwhile.
            const int now_busy=r.loader->busy;
            queue.erase(std::remove_if(queue.begin(),queue.end(),
                [&](const Request& q) { return q.index==now_busy; }),queue.end());
            r.loader->queue=std::move(queue);
            for (auto& decoded : keep) r.loader->done.push_back(std::move(decoded));
        }
        r.loader->wake.notify_one();
    }

    // 4. Draw the loaded wanted tiles. Other loaded tiles (finer ones left
    // over from zooming in, coarser ones from zooming out) only stand in
    // where an overlapping wanted tile is still streaming or broken; once
    // the selection is loaded, the picture depends on the view alone, not on
    // what happened to be cached, and where no tile is wanted the base
    // texture shows. Drawn finest level first (see globe_draw's stencil).
    std::vector<int> missing;
    for (int index : wanted)
        if (!r.cells[static_cast<size_t>(index)].image.id) missing.push_back(index);
    const auto overlaps=[&](const RasterTileRegion& a,const RasterTileRegion& b) {
        return a.u0<b.u1 && b.u0<a.u1 && a.v0<b.v1 && b.v0<a.v1;
    };
    std::vector<int> draw;
    for (const auto& c : visible) {
        const auto& cell=r.cells[static_cast<size_t>(c.index)];
        if (!cell.image.id) continue;
        const bool stand_in=cell.recent!=r.frame &&
            std::any_of(missing.begin(),missing.end(),[&](int index) {
                return overlaps(cell.region,r.cells[static_cast<size_t>(index)].region);
            });
        if (cell.recent==r.frame || stand_in) draw.push_back(c.index);
    }
    if (draw.empty()) return 0;
    std::stable_sort(draw.begin(),draw.end(),[&](int a,int b) {
        return r.cells[static_cast<size_t>(a)].level<r.cells[static_cast<size_t>(b)].level;
    });
    const bool copies=map.globe_to_flat>.999f && scene_map_seam_visible(map,view.aspect);
    for (int copy=(copies?-1:0);copy<=(copies?1:0);++copy) {
        scene_uniforms(shader,cam,view.aspect,map,static_cast<float>(copy));
        for (int index : draw) {
            auto& cell=r.cells[static_cast<size_t>(index)];
            bind_texture(cell.image,0);
            draw_mesh(cell.mesh,map.globe_to_flat>.001f);
        }
    }
    return static_cast<int>(draw.size());
}
