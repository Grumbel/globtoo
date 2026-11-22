#include "globe.hpp"
#include "scene.hpp"
#include "geocoordinates.hpp"
#include <cstdio>


bool globe_init(Globe& g, const char* texture_path) {
    // Extra tessellation keeps the texture/silhouette smooth at close zoom.
    // 257 * 129 vertices still fit GLES2 16-bit element indices.
    static_assert(257 * 129 <= 65536);
    g.mesh = create_sphere(256, 128);
    g.shader = load_shader("shaders/globe.vert", "shaders/globe.frag");
    if (!g.shader.program) {
        fprintf(stderr, "Failed to load shaders\n");
        return false;
    }
    g.texture = load_texture(texture_path);
    if (!g.texture.id) {
        fprintf(stderr, "Failed to load earth texture – using solid color fallback\n");
    }
    g.shading = true;
    return true;
}

void globe_shutdown(Globe& g) {
    destroy_mesh(g.mesh);
    destroy_shader(g.shader);
    destroy_texture(g.texture);
}

void globe_draw(const Globe& g, const Camera& cam, float aspect,
                const MapView& map) {
    const bool textured = g.texture_visible && g.texture.id != 0;
    Mat4 model = Mat4::identity();

    use_shader(g.shader);
    scene_uniforms(g.shader, cam, aspect, map);
    if (g.shader.u_model >= 0)
        glUniformMatrix4fv(g.shader.u_model, 1, GL_FALSE, model.m);
    if (g.shader.u_shading >= 0)
        glUniform1f(g.shader.u_shading, g.shading ? 1.f : 0.f);
    if (g.shader.u_light_dir >= 0) {
        // Headlight: rotates with the camera so the facing hemisphere stays lit
        Vec3 L = camera_headlight_dir(cam);
        glUniform3f(g.shader.u_light_dir, L.x, L.y, L.z);
    }

    GLint u_textured = glGetUniformLocation(g.shader.program, "u_textured");
    if (u_textured >= 0)
        glUniform1f(u_textured, textured ? 1.f : 0.f);

    if (textured && g.texture.id) {
        bind_texture(g.texture, 0);
        glUniform1i(g.shader.u_tex, 0);
    }

    const bool sphere = map.globe_to_flat < .001f;
    const bool flat = map.globe_to_flat > .999f;
    if (flat) glDisable(GL_DEPTH_TEST);
    else { glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); }
    glDepthMask(flat ? GL_FALSE : GL_TRUE);
    if (sphere) {
        glEnable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        glStencilFunc(GL_ALWAYS, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE);
    }
    // Reproject a single rotated Earth, not mirrored north/south map bands.
    // Additional horizontal copies are required only for wide flat views.
    const bool copies=flat && scene_map_seam_visible(map,aspect);
    for (int copy=(copies ? -1 : 0);copy<=(copies ? 1 : 0);copy++) {
        scene_uniforms(g.shader,cam,aspect,map,static_cast<float>(copy));
        draw_mesh(g.mesh,!sphere);
    }
    // Keep the stencil test enabled for political geometry. It limits the
    // overlay to the globe's visible silhouette, even where its slight radial
    // lift would otherwise make islands peek beyond the horizon.
    if (sphere) {
        glStencilMask(0x00);
        glStencilFunc(GL_EQUAL, 1, 0xff);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    }
    glDepthMask(GL_TRUE);
}
