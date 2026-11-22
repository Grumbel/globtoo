// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "tiled_raster.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
constexpr double pi=3.14159265358979323846;
}

bool raster_tiles_parse(std::string_view data, RasterTiles& out, std::string& error) {
    out={};
    std::istringstream stream{std::string(data)};
    std::string magic, trailing;
    int version=0;
    RasterTiles tmp{};
    if (!(stream >> magic >> version >> tmp.width >> tmp.height >>
          tmp.tile_size >> tmp.gutter >> tmp.columns >> tmp.rows) ||
        magic!="GLOBTOO_RASTER_TILES" || version!=1 || (stream >> trailing)) {
        error="invalid tiled raster manifest (expected GLOBTOO_RASTER_TILES 1)";
        return false;
    }
    // Put strict bounds on hostile/corrupt input before allocating GPU assets.
    if (tmp.width < 2 || tmp.height < 1 || tmp.width != 2LL * tmp.height ||
        tmp.width > 100000 || tmp.tile_size < 64 || tmp.tile_size > 2048 ||
        (tmp.tile_size & (tmp.tile_size-1)) ||
        tmp.gutter < 2 || tmp.gutter > 16 || tmp.content_size() <= 0 ||
        tmp.columns <= 0 || tmp.rows <= 0 || tmp.columns > 256 || tmp.rows > 256 ||
        (tmp.width + tmp.content_size()-1)/tmp.content_size() != tmp.columns ||
        (tmp.height + tmp.content_size()-1)/tmp.content_size() != tmp.rows ||
        tmp.tile_count() > 8192) {
        error="invalid dimensions or tile grid in raster manifest";
        return false;
    }
    out=tmp;
    error.clear();
    return true;
}

bool raster_tiles_read(const std::string& filename, RasterTiles& out, std::string& error) {
    std::ifstream file(filename);
    if (!file) { error="cannot open raster manifest: "+filename; return false; }
    std::string data((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
    if (data.size()>4096) { error="raster manifest exceeds size limit"; return false; }
    return raster_tiles_parse(data,out,error);
}

RasterTileRegion raster_tile_region(const RasterTiles& t, int col, int row) {
    if (col<0 || row<0 || col>=t.columns || row>=t.rows || t.width==0) return {};
    const int c=t.content_size();
    const int x0=col*c, y0=row*c;
    const int x1=std::min(x0+c,t.width), y1=std::min(y0+c,t.height);
    RasterTileRegion r{};
    r.u0=double(x0)/t.width; r.u1=double(x1)/t.width;
    r.v0=double(y0)/t.height; r.v1=double(y1)/t.height;
    r.tex_u0=double(t.gutter)/t.tile_size;
    r.tex_v0=double(t.gutter)/t.tile_size;
    r.tex_u1=double(t.gutter+x1-x0)/t.tile_size;
    r.tex_v1=double(t.gutter+y1-y0)/t.tile_size;
    r.lon=180.*(r.u0+r.u1)-180.;
    r.lat=90.-90.*(r.v0+r.v1);
    // Chord bound on the sphere: any point in this lon/lat rectangle is
    // within this angular radius of its midpoint. Conservative at poles.
    const double lon_half=pi*(r.u1-r.u0);
    const double lat_half=pi*.5*(r.v1-r.v0);
    r.angular_radius=std::min(pi,lon_half+lat_half);
    return r;
}

std::string raster_tile_filename(int col,int row) {
    std::ostringstream os;
    os << "tile-" << std::setfill('0') << std::setw(3) << row
       << '-' << std::setw(3) << col << ".jpg";
    return os.str();
}
