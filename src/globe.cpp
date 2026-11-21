#include "globe.hpp"
#include <cstdio>

bool globe_init(Globe& g, const char* texture_path) {
    g.mesh = create_sphere(128, 64);
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

void globe_draw(const Globe& g, const Camera& cam, float aspect, bool textured) {
    Mat4 proj  = cam.proj_matrix(aspect);
    Mat4 view  = cam.view_matrix();
    Mat4 model = Mat4::identity();
    Mat4 mvp   = Mat4::multiply(proj, Mat4::multiply(view, model));

    use_shader(g.shader);
    glUniformMatrix4fv(g.shader.u_mvp, 1, GL_FALSE, mvp.m);
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

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xff);
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    draw_mesh(g.mesh);
    // Keep the stencil test enabled for political geometry. It limits the
    // overlay to the globe's visible silhouette, even where its slight radial
    // lift would otherwise make islands peek beyond the horizon.
    glStencilMask(0x00);
    glStencilFunc(GL_EQUAL, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}
