// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "geographic_overlay.hpp"
#include "resources.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace {
constexpr std::size_t river_vertex_floats=9; // a_pos, a_other, a_corner
// 16-bit indices address at most 65536 vertices per draw.
constexpr GLushort river_chunk_quads=16384;
}

float river_width_km(unsigned scale_rank) {
    // Natural Earth ranks rivers by cartographic importance, not by measured
    // width. A rough typical channel width per rank keeps large rivers
    // visible against 500 m satellite imagery without turning tributaries
    // into ribbons: the Amazon is wider still, most tributaries are narrower.
    static constexpr float widths[]={2.f,2.f,1.5f,1.2f,.9f,.7f,.5f,.4f,.3f};
    return scale_rank<std::size(widths) ? widths[scale_rank] : .25f;
}

bool geographic_overlay_init(GeographicOverlay& overlay) {
    std::string error;
    if (!read_geographic_features(resource_path("assets/geographic_10m.gfeat"),
                                  overlay.features,&error)) {
        std::fprintf(stderr,"geography: optional city/river asset unavailable: %s\n",
                     error.c_str());
        return false;
    }
    overlay.river_shader=load_shader("shaders/rivers.vert","shaders/rivers.frag");
    if (!overlay.river_shader.program) return false;
    // Aggregate whole watercourses by rank into three VBOs. At distant zoom
    // only the main rivers need a draw call. The ribbon shader shares the
    // political border projection, including seam discontinuity protection.
    std::array<std::vector<float>,3> batches;
    for (const auto& seg:overlay.features.rivers) {
        const int tier=seg.scale_rank<=3 ? 0 : (seg.scale_rank<=6 ? 1 : 2);
        const float half_width=river_width_km(seg.scale_rank)*.5f/6371.f;
        const float min_half_px=(tier==0 ? 2.f : tier==1 ? 1.5f : 1.f)*.5f;
        auto& dst=batches[tier];
        float a[3],b[3];
        for (int k=0;k<3;k++) {a[k]=seg.a[k]*1.002f;b[k]=seg.b[k]*1.002f;}
        if (std::fabs(a[0]-b[0])+std::fabs(a[1]-b[1])+std::fabs(a[2]-b[2])<1e-7f) continue;
        // Corner order (start,-), (start,+), (end,-), (end,+).
        for (int end=0;end<2;end++) {
            const float* here=end ? b : a;
            const float* other=end ? a : b;
            for (float side : {-1.f,1.f}) {
                dst.insert(dst.end(),here,here+3);
                dst.insert(dst.end(),other,other+3);
                dst.push_back(side);
                dst.push_back(end ? -half_width : half_width);
                dst.push_back(min_half_px);
            }
        }
    }
    glGenBuffers(static_cast<GLsizei>(overlay.river_vbo.size()),overlay.river_vbo.data());
    for (std::size_t i=0;i<batches.size();i++) {
        glBindBuffer(GL_ARRAY_BUFFER,overlay.river_vbo[i]);
        glBufferData(GL_ARRAY_BUFFER,batches[i].size()*sizeof(float),
                     batches[i].data(),GL_STATIC_DRAW);
        overlay.river_quads[i]=static_cast<GLsizei>(batches[i].size()/(4*river_vertex_floats));
    }
    std::vector<GLushort> indices;
    indices.reserve(river_chunk_quads*6);
    for (GLushort q=0;q<river_chunk_quads;q++) {
        const GLushort v=static_cast<GLushort>(q*4);
        for (GLushort k : {0,1,2,2,1,3}) indices.push_back(static_cast<GLushort>(v+k));
    }
    glGenBuffers(1,&overlay.river_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,overlay.river_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size()*sizeof(GLushort),
                 indices.data(),GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    std::fprintf(stderr,"geography: %zu populated places and %zu river segments\n",
                 overlay.features.places.size(),overlay.features.rivers.size());
    return true;
}

void geographic_overlay_shutdown(GeographicOverlay& overlay) {
    glDeleteBuffers(static_cast<GLsizei>(overlay.river_vbo.size()),overlay.river_vbo.data());
    glDeleteBuffers(1,&overlay.river_ibo);
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
        glStencilFunc(GL_EQUAL,1,0x01); // bit 0: globe silhouette
        glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
    } else glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_FALSE);
    if (flat) glDisable(GL_DEPTH_TEST);
    else {glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);}
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    use_shader(overlay.river_shader);
    const Shader& shader=overlay.river_shader;
    if (shader.u_color>=0) glUniform4f(shader.u_color,.20f,.56f,.84f,.92f);
    if (shader.u_viewport>=0) {
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT,viewport);
        glUniform2f(shader.u_viewport,static_cast<float>(viewport[2]),
                    static_cast<float>(viewport[3]));
    }
    for (GLuint attribute : {0u,2u,4u}) glEnableVertexAttribArray(attribute);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,overlay.river_ibo);
    const unsigned rank=geographic_detail_rank(camera.distance,map.zoom,map.flat_goal>.5f);
    const std::size_t tiers=rank<=3 ? 1 : rank<=6 ? 2 : 3;
    const bool copies=flat && scene_map_seam_visible(map,aspect);
    constexpr GLsizei stride=river_vertex_floats*sizeof(float);
    for (int copy=copies ? -1 : 0;copy<=(copies ? 1 : 0);copy++) {
        scene_uniforms(shader,camera,aspect,map,static_cast<float>(copy));
        for (std::size_t i=0;i<tiers;i++) {
            glBindBuffer(GL_ARRAY_BUFFER,overlay.river_vbo[i]);
            for (GLsizei first=0;first<overlay.river_quads[i];first+=river_chunk_quads) {
                const GLsizei quads=std::min<GLsizei>(river_chunk_quads,
                                                      overlay.river_quads[i]-first);
                const std::uintptr_t base=static_cast<std::uintptr_t>(first)*4*stride;
                for (auto [attribute,offset] : {std::pair{0u,0u},{2u,3u},{4u,6u}})
                    glVertexAttribPointer(attribute,3,GL_FLOAT,GL_FALSE,stride,
                        reinterpret_cast<const void*>(base+offset*sizeof(float)));
                glDrawElements(GL_TRIANGLES,quads*6,GL_UNSIGNED_SHORT,nullptr);
            }
        }
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    for (GLuint attribute : {0u,2u,4u}) glDisableVertexAttribArray(attribute);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDepthMask(GL_TRUE);
}
