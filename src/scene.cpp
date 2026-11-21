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
                    const MapView& map, float wrap) {
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
}

bool scene_project(const Camera& camera, float aspect, const MapView& map,
                   double lon_deg, double lat_deg, float& x, float& y) {
    const auto sphere = geo::lonlat_to_xyz(lon_deg, lat_deg, 1.0015);
    const Mat4 view = camera.view_matrix();
    const auto pos = mul(view, {static_cast<float>(sphere[0]),
                                static_cast<float>(sphere[1]),
                                static_cast<float>(sphere[2]),1.f});
    // Cull hidden back hemisphere when globe is fully spherical.
    const Vec3 forward = camera_view_to_world(camera, {0,0,1});
    if (sphere[0]*forward.x + sphere[1]*forward.y + sphere[2]*forward.z <
        .2 - 1.2*map.globe_to_flat) return false;
    const double lon = (lon_deg*geo::to_rad - map.center_lon);
    if (map.globe_to_flat > .95f && std::abs(lon) > geo::pi) return false;
    const auto p1 = map_project(map.from, lon, lat_deg * geo::to_rad);
    const auto p2 = map_project(map.to, lon, lat_deg * geo::to_rad);
    const auto center1 = map_project(map.from, 0, map.center_lat);
    const auto center2 = map_project(map.to, 0, map.center_lat);
    const double t = map.projection_blend;
    const double mx = ((p1[0]-center1[0])*(1-t) + (p2[0]-center2[0])*t)*map.zoom;
    const double my = ((p1[1]-center1[1])*(1-t) + (p2[1]-center2[1])*t)*map.zoom;
    const double blend = map.globe_to_flat;
    Vec4 blendpos{static_cast<float>(pos.x*(1-blend)+mx*blend),
                  static_cast<float>(pos.y*(1-blend)+my*blend),
                  static_cast<float>(pos.z*(1-blend)-map.map_distance*blend), 1.f};
    const Vec4 clip = mul(scene_projection(camera, aspect, map), blendpos);
    if (clip.w <= 0.f || std::abs(clip.x)>clip.w || std::abs(clip.y)>clip.w ||
        clip.z< -clip.w || clip.z>clip.w) return false;
    x = clip.x / clip.w; y = clip.y / clip.w;
    return true;
}

bool scene_map_seam_visible(const MapView& map, float aspect) {
    // This test is conservative for oval projections: visible map half-width
    // can shrink with latitude, but never exceeds the equirectangular scale.
    const double half_width = std::tan(.35) * map.map_distance * aspect /
                              std::max(.6f,map.zoom);
    return half_width >= geo::pi * .97;
}
