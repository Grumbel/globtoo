#include "countries.hpp"
#include "country_geometry.hpp"
#include "country_lod.hpp"
#include "country_mesh_io.hpp"
#include "scene.hpp"
#include "geocoordinates.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> asset_paths(const char* asset_name) {
    std::vector<std::string> paths;
    if (const char* dir = std::getenv("GLOBE_ASSET_DIR")) {
        paths.emplace_back(std::string(dir) + "/assets/" + asset_name);
        paths.emplace_back(std::string(dir) + "/" + asset_name);
    }
    paths.emplace_back(std::string("assets/") + asset_name);
    return paths;
}

bool read_geojson(const std::string& path, CountryGeometry& geometry) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream text;
    text << file.rdbuf();
    std::string error;
    if (!build_country_geometry(text.str(), geometry, &error)) {
        fprintf(stderr, "countries: %s (%s)\n", error.c_str(), path.c_str());
        return false;
    }
    return true;
}

bool upload(CountryMeshGPU& gpu, const CountryGeometry& geo, const char* label) {
    if (geo.border_xyz.empty() || geo.fill_xyz_rgb.empty() ||
        geo.border_lonlat.size() != geo.border_xyz.size()/3*2 ||
        geo.fill_lonlat.size() != geo.fill_xyz_rgb.size()/6*2) return false;
    gpu.labels = geo.labels;

    std::vector<float> borders, fills;
    const std::size_t nb = geo.border_xyz.size()/3;
    // Store every border *segment* with one shared geographic anchor. A strip
    // cannot cross the moving reprojection seam without a discontinuity.
    auto anchor_for = [](const auto& points, auto index, auto stride,
                         std::size_t n) {
        std::array<float,3> anchor{};
        for (size_t j=0;j<n;j++)
            for (size_t k=0;k<3;k++) anchor[k]+=points[(index+j)*stride+k];
        const float length=std::sqrt(anchor[0]*anchor[0]+anchor[1]*anchor[1]+anchor[2]*anchor[2]);
        if (length>1e-8f) for (float& a:anchor) a/=length;
        else for (size_t k=0;k<3;k++) anchor[k]=points[index*stride+k];
        return anchor;
    };
    borders.reserve(nb*12);
    for (const auto& strip:geo.border_strips) {
        for (std::size_t j=1;j<static_cast<std::size_t>(strip.count);j++) {
            const std::size_t i=strip.first+j-1;
            const std::array<float,3> anchor={
                geo.border_xyz[i*3]+geo.border_xyz[(i+1)*3],
                geo.border_xyz[i*3+1]+geo.border_xyz[(i+1)*3+1],
                geo.border_xyz[i*3+2]+geo.border_xyz[(i+1)*3+2]};
            const float len=std::sqrt(anchor[0]*anchor[0]+anchor[1]*anchor[1]+anchor[2]*anchor[2]);
            const std::array<float,3> normalized=len>1e-8f ?
                std::array<float,3>{anchor[0]/len,anchor[1]/len,anchor[2]/len} :
                std::array<float,3>{geo.border_xyz[i*3],geo.border_xyz[i*3+1],geo.border_xyz[i*3+2]};
            for (std::size_t k=i;k<=i+1;k++) {
                borders.insert(borders.end(),geo.border_xyz.begin()+k*3,geo.border_xyz.begin()+k*3+3);
                borders.insert(borders.end(),normalized.begin(),normalized.end());
            }
        }
    }
    gpu.border_count=static_cast<GLsizei>(borders.size()/6);

    const std::size_t nf = geo.fill_xyz_rgb.size()/6;
    fills.reserve(nf*9);
    for (std::size_t i=0;i<nf;i+=3) {
        const auto anchor=anchor_for(geo.fill_xyz_rgb,i,6,3);
        for (std::size_t j=i;j<i+3;j++) {
            fills.insert(fills.end(),geo.fill_xyz_rgb.begin()+j*6,geo.fill_xyz_rgb.begin()+j*6+6);
            fills.insert(fills.end(),anchor.begin(),anchor.end());
        }
    }
    glGenBuffers(1, &gpu.border_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.border_vbo);
    glBufferData(GL_ARRAY_BUFFER, borders.size() * sizeof(float), borders.data(), GL_STATIC_DRAW);
    gpu.fill_count = static_cast<GLsizei>(fills.size()/9);
    glGenBuffers(1, &gpu.fill_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gpu.fill_vbo);
    glBufferData(GL_ARRAY_BUFFER, fills.size() * sizeof(float), fills.data(), GL_STATIC_DRAW);
    fprintf(stderr, "countries %s: %zu features, %zu polygons, %zu rings, %d fill vertices, %zu English names, %zu failures\n",
            label, geo.feature_count, geo.polygon_count, geo.border_strips.size(),
            static_cast<int>(gpu.fill_count),gpu.labels.size(),geo.triangulation_failures);
    return gpu.available();
}

void destroy(CountryMeshGPU& gpu) {
    if (gpu.border_vbo) glDeleteBuffers(1, &gpu.border_vbo);
    if (gpu.fill_vbo) glDeleteBuffers(1, &gpu.fill_vbo);
    gpu = {};
}

bool load_compiled(CountryMeshGPU& gpu, const char* asset_name, const char* label) {
    for (const auto& path : asset_paths(asset_name)) {
        std::ifstream file(path, std::ios::binary);
        if (!file) continue;
        file.close();
        CountryGeometry geometry;
        std::string error;
        if (!read_country_mesh(path, geometry, &error)) {
            fprintf(stderr, "countries: %s (%s)\n", error.c_str(), path.c_str());
            return false;
        }
        return upload(gpu, geometry, label);
    }
    return false;
}
} // namespace

bool countries_init(Countries& c, const char* geojson_path) {
    if (geojson_path && *geojson_path) {
        CountryGeometry geometry;
        if (!read_geojson(geojson_path, geometry) ||
            !upload(c.low, geometry, "custom")) return false;
    } else {
        load_compiled(c.low, "countries_50m.gmesh", "50m");
        load_compiled(c.high, "countries_10m.gmesh", "10m");
        // Backwards compatibility: local development can still provide just
        // assets/countries.geojson, without running the mesh compiler.
        if (!c.low.available() && !c.high.available()) {
            for (const auto& path : asset_paths("countries.geojson")) {
                CountryGeometry geometry;
                if (read_geojson(path, geometry)) {
                    if (upload(c.low, geometry, "GeoJSON")) break;
                }
            }
        }
    }
    if (!c.low.available() && !c.high.available()) {
        fprintf(stderr, "countries: no valid country meshes found\n");
        return false;
    }
    c.border_shader = load_shader("shaders/borders.vert", "shaders/borders.frag");
    c.fill_shader = load_shader("shaders/fill.vert", "shaders/fill.frag");
    if (!c.border_shader.program || !c.fill_shader.program) {
        fprintf(stderr, "countries: shader failed\n");
        countries_shutdown(c);
        return false;
    }
    return true;
}

void countries_shutdown(Countries& c) {
    destroy(c.low);
    destroy(c.high);
    destroy_shader(c.border_shader);
    destroy_shader(c.fill_shader);
    c = {};
}

void countries_draw(Countries& c, const Camera& cam, float aspect,
                    const MapView& map, bool shading, float fill_opacity) {
    c.using_high = country_use_high_detail(
        map.flat_goal > .5f ? std::clamp(3.2f / map.zoom, 1.2f, 3.2f) : cam.distance,
        c.using_high, c.high.available(), c.low.available());
    const CountryMeshGPU& gpu = c.using_high ? c.high : c.low;
    if (!gpu.available()) { glDisable(GL_STENCIL_TEST); return; }

    const bool sphere = map.globe_to_flat < .001f;
    const bool flat = map.globe_to_flat > .999f;
    glDepthMask(GL_FALSE);
    if (flat) glDisable(GL_DEPTH_TEST);
    else { glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); }
    if (sphere) {
        glEnable(GL_CULL_FACE); glCullFace(GL_BACK); glFrontFace(GL_CCW);
    } else {
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
    }

    const bool copies=flat && scene_map_seam_visible(map,aspect);
    for (int copy=(copies ? -1 : 0);copy<=(copies ? 1 : 0);copy++) {
        const float wrap=static_cast<float>(copy);
        if (c.fill_visible && c.fill_shader.program) {
            // Blend political colors over imagery. Only this pass blends:
            // borders remain fully opaque, and globe depth/stencil still
            // occlude country geometry on the far side of the sphere.
            if (fill_opacity < 1.f) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            use_shader(c.fill_shader);
            scene_uniforms(c.fill_shader,cam,aspect,map,wrap);
            const GLint u_light=glGetUniformLocation(c.fill_shader.program,"u_light_dir");
            const GLint u_shading=glGetUniformLocation(c.fill_shader.program,"u_shading");
            if (u_light>=0) {
                const Vec3 light=camera_headlight_dir(cam);
                glUniform3f(u_light,light.x,light.y,light.z);
            }
            if (u_shading>=0) glUniform1f(u_shading,shading ? 1.f : 0.f);
            const GLint u_opacity=glGetUniformLocation(c.fill_shader.program,"u_fill_opacity");
            if (u_opacity>=0) glUniform1f(u_opacity,fill_opacity);
            glBindBuffer(GL_ARRAY_BUFFER,gpu.fill_vbo);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)0);
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(3*sizeof(float)));
            glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,9*sizeof(float),(void*)(6*sizeof(float)));
            glDrawArrays(GL_TRIANGLES,0,gpu.fill_count);
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(3);
            if (fill_opacity < 1.f) glDisable(GL_BLEND);
        }
        if (c.borders_visible && c.border_shader.program) {
            use_shader(c.border_shader);
            scene_uniforms(c.border_shader,cam,aspect,map,wrap);
            const GLint u_color=glGetUniformLocation(c.border_shader.program,"u_color");
            if (u_color>=0) glUniform4f(u_color,.12f,.10f,.08f,1.f);
            glLineWidth(1.f);
            glBindBuffer(GL_ARRAY_BUFFER,gpu.border_vbo);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
            glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)(3*sizeof(float)));
            glDrawArrays(GL_LINES,0,gpu.border_count);
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(3);
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}
