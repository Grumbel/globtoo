#include "scene.hpp"
#include "geocoordinates.hpp"
#include <algorithm>
#include <cmath>

namespace {
void setf(GLuint p, const char* name, float value) {
    GLint loc=glGetUniformLocation(p,name);
    if (loc>=0) glUniform1f(loc,value);
}
void setvec(GLuint p, const char* name, const std::array<double,3>& v) {
    GLint loc=glGetUniformLocation(p,name);
    if (loc>=0) glUniform3f(loc,static_cast<float>(v[0]),
                          static_cast<float>(v[1]),static_cast<float>(v[2]));
}
void setmat(GLuint p, const char* name, const Mat4& m) {
    GLint loc=glGetUniformLocation(p,name);
    if (loc>=0) glUniformMatrix4fv(loc,1,GL_FALSE,m.m);
}
Mat4 scene_projection(const Camera& camera, float aspect, const MapView& map) {
    if (map.globe_to_flat<=.0001f) return camera.proj_matrix(aspect);
    const float near_plane=std::max(.05f,(camera.distance-1.01f)*.5f);
    const float far_plane=std::max(9.f,camera.distance+1.1f);
    return Mat4::perspective(.7f,aspect,near_plane,far_plane);
}
Vec4 mul(const Mat4& a, Vec4 p) {
    return {a.m[0]*p.x+a.m[4]*p.y+a.m[8]*p.z+a.m[12]*p.w,
            a.m[1]*p.x+a.m[5]*p.y+a.m[9]*p.z+a.m[13]*p.w,
            a.m[2]*p.x+a.m[6]*p.y+a.m[10]*p.z+a.m[14]*p.w,
            a.m[3]*p.x+a.m[7]*p.y+a.m[11]*p.z+a.m[15]*p.w};
}
} // namespace

void scene_uniforms(const Shader& shader, const Camera& camera, float aspect,
                    const MapView& map, float wrap) {
    setmat(shader.program,"u_view",camera.view_matrix());
    setmat(shader.program,"u_projection",scene_projection(camera,aspect,map));
    setf(shader.program,"u_morph",map.globe_to_flat);
    setf(shader.program,"u_projection_from",static_cast<float>(static_cast<int>(map.from)));
    setf(shader.program,"u_projection_to",static_cast<float>(static_cast<int>(map.to)));
    setf(shader.program,"u_projection_blend",map.projection_blend);
    const auto frame=map_frame(map);
    setvec(shader.program,"u_map_east",frame.east);
    setvec(shader.program,"u_map_north",frame.north);
    setvec(shader.program,"u_map_center",frame.center);
    setf(shader.program,"u_map_zoom",map.zoom);
    setf(shader.program,"u_map_distance",map.map_distance);
    setf(shader.program,"u_wrap",wrap);
    const auto oval=[](MapProjection p) {
        return p==MapProjection::Mollweide || p==MapProjection::Hammer;
    };
    setf(shader.program,"u_bounded_projection",oval(map.from)||oval(map.to) ? 1.f : 0.f);
    setf(shader.program,"u_map_roll",static_cast<float>(map.roll));
    const GLint pan=glGetUniformLocation(shader.program,"u_map_pan");
    if (pan>=0) glUniform2f(pan,static_cast<float>(map.pan_x),
                           static_cast<float>(map.pan_y));
}

bool scene_project(const Camera& camera, float aspect, const MapView& map,
                   double lon_deg, double lat_deg, float& x, float& y) {
    const auto xyz=geo::lonlat_to_xyz(lon_deg,lat_deg,1.0015);
    const auto sphere=mul(camera.view_matrix(),
         {static_cast<float>(xyz[0]),static_cast<float>(xyz[1]),
          static_cast<float>(xyz[2]),1.f});
    const Vec3 forward=camera_view_to_world(camera,{0,0,1});
    if (xyz[0]*forward.x+xyz[1]*forward.y+xyz[2]*forward.z <
        .2-1.2*map.globe_to_flat) return false;
    const auto ll=map_relative_lonlat(map,lon_deg*geo::to_rad,lat_deg*geo::to_rad);
    const double t=map.projection_blend;
    const bool bounded=map.from==MapProjection::Mollweide ||
                       map.from==MapProjection::Hammer ||
                       map.to==MapProjection::Mollweide ||
                       map.to==MapProjection::Hammer;
    bool found=false;
    float best_d=1e30f;
    const bool flat=map.globe_to_flat > .999f;
    for (int copy=-1;copy<=1;copy++) {
        if (copy && (!flat || !scene_map_seam_visible(map,aspect))) continue;
        const double copy_lon=ll[0]+copy*2*geo::pi;
        if (flat && bounded && std::abs(copy_lon)>geo::pi) continue;
        const auto cf=map_project(map.from,copy_lon,ll[1]);
        const auto ct=map_project(map.to,copy_lon,ll[1]);
        const double cx=(1.-t)*cf[0]+t*ct[0];
        const double cy=(1.-t)*cf[1]+t*ct[1];
        const double cr=std::cos(map.roll),sr=std::sin(map.roll);
        const double mx=(cr*cx-sr*cy)*map.zoom+map.pan_x;
        const double my=(sr*cx+cr*cy)*map.zoom+map.pan_y;
        const double morph=map.globe_to_flat;
        const Vec4 p{static_cast<float>(sphere.x*(1-morph)+mx*morph),
                     static_cast<float>(sphere.y*(1-morph)+my*morph),
                     static_cast<float>(sphere.z*(1-morph)-map.map_distance*morph),1.f};
        const Vec4 clip=mul(scene_projection(camera,aspect,map),p);
        if (clip.w<=0 || std::abs(clip.x)>clip.w || std::abs(clip.y)>clip.w ||
            clip.z < -clip.w || clip.z>clip.w) continue;
        const float nx=clip.x/clip.w,ny=clip.y/clip.w;
        const float d=nx*nx+ny*ny;
        if (!found || d<best_d) { best_d=d; x=nx; y=ny; found=true; }
    }
    return found;
}

bool scene_map_seam_visible(const MapView& map,float aspect) {
    const double h=std::tan(.35)*map.map_distance/std::max(.6f,map.zoom);
    const double extent=h*(std::abs(std::cos(map.roll))*aspect+
                           std::abs(std::sin(map.roll)));
    const double t=map.projection_blend;
    const double half_width=.5*((1.-t)*map_world_width(map.from)+
                                  t*map_world_width(map.to));
    const double center_shift=std::abs(std::cos(map.roll)*map.pan_x+
                                       std::sin(map.roll)*map.pan_y)/map.zoom;
    return extent+center_shift>=half_width*.97;
}
