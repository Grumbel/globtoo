// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "geographic_overlay.hpp"
#include "resources.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

bool geographic_overlay_init(GeographicOverlay& overlay) {
    std::string error;
    if (!read_geographic_features(resource_path("assets/geographic_10m.gfeat"),
                                  overlay.features,&error)) {
        std::fprintf(stderr,"geography: optional city/river asset unavailable: %s\n",
                     error.c_str());
        return false;
    }
    overlay.river_shader=load_shader("shaders/borders.vert","shaders/borders.frag");
    if (!overlay.river_shader.program) return false;
    // Aggregate whole watercourses by rank into three VBOs. At distant zoom
    // only the main rivers need a draw call. Lines share the political border
    // shader, including rotating frame / seam discontinuity protection.
    std::array<std::vector<float>,3> batches;
    for (const auto& seg:overlay.features.rivers) {
        const int tier=seg.scale_rank<=3 ? 0 : (seg.scale_rank<=6 ? 1 : 2);
        auto& dst=batches[tier];
        const float x=seg.a[0]+seg.b[0],y=seg.a[1]+seg.b[1],z=seg.a[2]+seg.b[2];
        const float len=std::sqrt(x*x+y*y+z*z);
        if (len < .0001f) continue;
        const float anchor[3]={x/len,y/len,z/len};
        for (const float* endpoint : {seg.a,seg.b}) {
            for (int k=0;k<3;k++) dst.push_back(endpoint[k]*1.002f);
            dst.insert(dst.end(),anchor,anchor+3);
        }
    }
    glGenBuffers(static_cast<GLsizei>(overlay.river_vbo.size()),overlay.river_vbo.data());
    for (std::size_t i=0;i<batches.size();i++) {
        glBindBuffer(GL_ARRAY_BUFFER,overlay.river_vbo[i]);
        glBufferData(GL_ARRAY_BUFFER,batches[i].size()*sizeof(float),
                     batches[i].data(),GL_STATIC_DRAW);
        overlay.river_vertices[i]=static_cast<GLsizei>(batches[i].size()/6);
    }
    std::fprintf(stderr,"geography: %zu populated places and %zu river segments\n",
                 overlay.features.places.size(),overlay.features.rivers.size());
    return true;
}

void geographic_overlay_shutdown(GeographicOverlay& overlay) {
    glDeleteBuffers(static_cast<GLsizei>(overlay.river_vbo.size()),overlay.river_vbo.data());
    destroy_shader(overlay.river_shader);
    overlay={};
}

void geographic_overlay_draw_rivers(const GeographicOverlay& overlay,
                                    const Camera& camera,float aspect,
                                    const MapView& map,bool visible) {
    if (!visible || !overlay.river_shader.program) return;
    const bool sphere=map.globe_to_flat<.001f;
    const bool flat=map.globe_to_flat>.999f;
    if (sphere) {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0);
        glStencilFunc(GL_EQUAL,1,0xff);
        glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    } else glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);
    if (flat) glDisable(GL_DEPTH_TEST);
    else {glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);}
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    use_shader(overlay.river_shader);
    if (overlay.river_shader.u_color>=0)
        glUniform4f(overlay.river_shader.u_color,.20f,.62f,.86f,.80f);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(3);
    glLineWidth(1.f);
    const unsigned rank=geographic_detail_rank(camera.distance,map.zoom,map.flat_goal>.5f);
    const std::size_t tiers=rank<=3 ? 1 : rank<=6 ? 2 : 3;
    const bool copies=flat && scene_map_seam_visible(map,aspect);
    for (std::size_t i=0;i<tiers;i++) {
        if (!overlay.river_vertices[i]) continue;
        glBindBuffer(GL_ARRAY_BUFFER,overlay.river_vbo[i]);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,6*sizeof(float),(void*)0);
        glVertexAttribPointer(3,3,GL_FLOAT,GL_FALSE,6*sizeof(float),
                              (void*)(3*sizeof(float)));
        for (int copy=copies ? -1 : 0;copy<=(copies ? 1 : 0);copy++) {
            scene_uniforms(overlay.river_shader,camera,aspect,map,static_cast<float>(copy));
            glDrawArrays(GL_LINES,0,overlay.river_vertices[i]);
        }
    }
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(3);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
}
