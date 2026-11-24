// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "planet_rings.hpp"
#include "ring_profile.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

bool planet_rings_init(PlanetRings& rings, BodyId body) {
    const RingProfile profile=ring_profile(body);
    if (!profile.valid()) return false;
    const double radius_km=body_info(body).mean_radius_km;
    rings.body=body;
    rings.inner=static_cast<float>(profile.inner_km/radius_km);
    rings.outer=static_cast<float>(profile.outer_km/radius_km);
    rings.shader=load_shader("shaders/rings.vert","shaders/rings.frag");
    if (!rings.shader.program) {
        std::fprintf(stderr,"rings: failed to load shaders\n");
        return false;
    }
    rings.u_eye=glGetUniformLocation(rings.shader.program,"u_eye");
    rings.u_ring_range=glGetUniformLocation(rings.shader.program,"u_ring_range");
    rings.u_fade=glGetUniformLocation(rings.shader.program,"u_fade");
    ImageRGBA image;
    image.width=profile.width;
    image.height=1;
    image.pixels=profile.rgba;
    rings.texture=upload_texture_rgba(image,false);
    // A triangle strip annulus. The fragment shader computes the exact
    // radius per pixel, so the polygon only has to cover the circles: its
    // outer edge is pushed out to circumscribe the outer circle.
    constexpr int segments=256;
    const float pi=3.14159265358979f;
    const float inner=rings.inner*.999f;
    const float outer=rings.outer/std::cos(pi/segments);
    std::vector<float> strip;
    strip.reserve((segments+1)*6);
    for (int i=0;i<=segments;i++) {
        const float angle=2.f*pi*static_cast<float>(i%segments)/segments;
        const float c=std::cos(angle), s=std::sin(angle);
        for (float r : {inner,outer}) {
            strip.push_back(r*c);
            strip.push_back(0.f);
            strip.push_back(-r*s);
        }
    }
    glGenBuffers(1,&rings.vbo);
    glBindBuffer(GL_ARRAY_BUFFER,rings.vbo);
    glBufferData(GL_ARRAY_BUFFER,strip.size()*sizeof(float),strip.data(),GL_STATIC_DRAW);
    rings.vertices=static_cast<GLsizei>(strip.size()/3);
    return true;
}

void planet_rings_shutdown(PlanetRings& rings) {
    glDeleteBuffers(1,&rings.vbo);
    destroy_texture(rings.texture);
    destroy_shader(rings.shader);
    rings={};
}

float planet_rings_content_radius(const PlanetRings& rings, BodyId body) {
    return rings.valid() && body==rings.body ? std::max(1.f,rings.outer) : 1.f;
}

void planet_rings_draw(const PlanetRings& rings, BodyId body, const Camera& camera,
                       float aspect, const MapView& map) {
    if (!rings.valid() || body!=rings.body) return;
    // A flat map has no equatorial plane to put the rings in. Fade them out
    // quickly at the start of a morph, before the globe visibly deforms.
    const float fade=1.f-std::clamp(map.globe_to_flat*4.f,0.f,1.f);
    if (fade<=0.f) return;
    use_shader(rings.shader);
    // Same view/projection as the globe, so its depth occludes the far side.
    scene_uniforms(rings.shader,camera,aspect,map);
    const Vec3 eye=camera_view_to_world(camera,{0.f,0.f,camera.distance});
    const Vec3 light=camera_headlight_dir(camera);
    if (rings.u_eye>=0) glUniform3f(rings.u_eye,eye.x,eye.y,eye.z);
    if (rings.shader.u_light_dir>=0)
        glUniform3f(rings.shader.u_light_dir,light.x,light.y,light.z);
    if (rings.u_ring_range>=0) glUniform2f(rings.u_ring_range,rings.inner,rings.outer);
    if (rings.u_fade>=0) glUniform1f(rings.u_fade,fade);
    if (rings.shader.u_tex>=0) glUniform1i(rings.shader.u_tex,0);
    bind_texture(rings.texture,0);
    // Rings extend beyond the globe silhouette; overlays drawn afterwards
    // still expect globe_draw's silhouette stencil test.
    const GLboolean stencil=glIsEnabled(GL_STENCIL_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);    // visible from both sides
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glBindBuffer(GL_ARRAY_BUFFER,rings.vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);
    glDrawArrays(GL_TRIANGLE_STRIP,0,rings.vertices);
    glDisableVertexAttribArray(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    if (stencil) glEnable(GL_STENCIL_TEST);
}
