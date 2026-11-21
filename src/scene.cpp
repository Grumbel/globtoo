#include "scene.hpp"
#include "geocoordinates.hpp"
#include <algorithm>
#include <cmath>

namespace {
void setf(GLuint p, const char* name, float value) {
    GLint loc = glGetUniformLocation(p, name);
    if (loc >= 0) glUniform1f(loc, value);
}
void setmat(GLuint p, const char* name, const Mat4& m) {
    GLint loc = glGetUniformLocation(p, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, m.m);
}
Mat4 scene_projection(const Camera& camera, float aspect, const MapView& map) {
    if (map.globe_to_flat <= .0001f) return camera.proj_matrix(aspect);
    const float near_plane = std::max(.05f, (camera.distance-1.01f)*.5f);
    const float far_plane = std::max(9.f, camera.distance + 1.1f);
    return Mat4::perspective(.7f, aspect, near_plane, far_plane);
}
Vec4 mul(const Mat4& a, Vec4 p) {
    return {a.m[0]*p.x+a.m[4]*p.y+a.m[8]*p.z+a.m[12]*p.w,
            a.m[1]*p.x+a.m[5]*p.y+a.m[9]*p.z+a.m[13]*p.w,
            a.m[2]*p.x+a.m[6]*p.y+a.m[10]*p.z+a.m[14]*p.w,
            a.m[3]*p.x+a.m[7]*p.y+a.m[11]*p.z+a.m[15]*p.w};
}
}

void scene_uniforms(const Shader& shader, const Camera& camera, float aspect,
                    const MapView& map, float wrap, int lat_band) {
    const Mat4 view = camera.view_matrix();
    const Mat4 projection = scene_projection(camera, aspect, map);
    setmat(shader.program, "u_view", view);
    setmat(shader.program, "u_projection", projection);
    setf(shader.program, "u_morph", map.globe_to_flat);
    setf(shader.program, "u_projection_from", static_cast<float>(static_cast<int>(map.from)));
    setf(shader.program, "u_projection_to", static_cast<float>(static_cast<int>(map.to)));
    setf(shader.program, "u_projection_blend", map.projection_blend);
    setf(shader.program, "u_center_lon", static_cast<float>(map.center_lon));
    setf(shader.program, "u_center_lat", static_cast<float>(map.center_lat));
    setf(shader.program, "u_map_zoom", map.zoom);
    setf(shader.program, "u_map_distance", map.map_distance);
    setf(shader.program, "u_wrap", wrap);
    setf(shader.program, "u_lat_band", static_cast<float>(lat_band));
    setf(shader.program, "u_map_roll", static_cast<float>(map.roll));
}

bool scene_project(const Camera& camera, float aspect, const MapView& map,
                   double lon_deg, double lat_deg, float& x, float& y) {
    const auto sphere = geo::lonlat_to_xyz(lon_deg, lat_deg, 1.0015);
    const Mat4 view = camera.view_matrix();
    const auto pos = mul(view, {static_cast<float>(sphere[0]),
                                static_cast<float>(sphere[1]),
                                static_cast<float>(sphere[2]),1.f});
    const Vec3 forward = camera_view_to_world(camera, {0,0,1});
    if (sphere[0]*forward.x + sphere[1]*forward.y + sphere[2]*forward.z <
        .2 - 1.2*map.globe_to_flat) return false;
    const bool flat = map.globe_to_flat > .999f;
    const auto bands = flat ? map_visible_bands(map,aspect) : std::vector<int>{0};
    const double wraps[] = {-2*geo::pi,0,2*geo::pi};
    const double t = map.projection_blend;
    bool found = false;
    float best_d = 1e30f;
    for (int band : bands) {
      const bool repeated_x = flat && scene_map_seam_visible(map,aspect,band);
      for (int w=0;w<(repeated_x ? 3 : 1);w++) {
        const double lon = lon_deg*geo::to_rad - map.center_lon - band*geo::pi +
                           (repeated_x ? wraps[w] : 0.);
        if (flat && std::abs(lon)>geo::pi) continue;
        const double lat = lat_deg * geo::to_rad;
        const double flip = band%2 == 0 ? 1. : -1.;
        const auto from = map_project(map.from,lon,lat);
        const auto to = map_project(map.to,lon,lat);
        const auto fy = 2*band*map_project(map.from,0,geo::pi*.5)[1] + flip*from[1] -
                        map_sheet_y(map.from,map.center_lat);
        const auto ty = 2*band*map_project(map.to,0,geo::pi*.5)[1] + flip*to[1] -
                        map_sheet_y(map.to,map.center_lat);
        const double px = ((1-t)*from[0]+t*to[0]);
        const double py = ((1-t)*fy+t*ty);
        const double cr = std::cos(map.roll), sr = std::sin(map.roll);
        const double mx = (cr*px-sr*py)*map.zoom;
        const double my = (sr*px+cr*py)*map.zoom;
        const double morph = map.globe_to_flat;
        const Vec4 p{static_cast<float>(pos.x*(1-morph)+mx*morph),
                     static_cast<float>(pos.y*(1-morph)+my*morph),
                     static_cast<float>(pos.z*(1-morph)-map.map_distance*morph),1.f};
        const Vec4 clip = mul(scene_projection(camera,aspect,map),p);
        if (clip.w<=0 || std::abs(clip.x)>clip.w || std::abs(clip.y)>clip.w ||
            clip.z < -clip.w || clip.z > clip.w) continue;
        const float nx = clip.x/clip.w, ny = clip.y/clip.w;
        const float dist = nx*nx+ny*ny;
        if (!found || dist<best_d) { best_d=dist; x=nx; y=ny; found=true; }
      }
    }
    return found;
}

bool scene_map_seam_visible(const MapView& map, float aspect, int lat_band) {
    // With map roll, the horizontal map extent grows with viewport height.
    const double h = std::tan(.35) * map.map_distance / std::max(.6f,map.zoom);
    const double extent = h * (std::abs(std::cos(map.roll))*aspect +
                               std::abs(std::sin(map.roll)));
    // k odd means we look across the opposite meridian. If that meridian
    // lies at the source geometry's antimeridian, draw both +360/-360 copies
    // even when zoomed far in: otherwise countries disappear near the pole.
    const double physical_center = std::abs(std::remainder(map.center_lon +
                                                         lat_band*geo::pi,2*geo::pi));
    return physical_center + extent >= geo::pi * .97;
}
