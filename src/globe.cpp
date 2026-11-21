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

void globe_draw(const Globe& g, const Camera& cam, float aspect) {
    Mat4 proj  = cam.proj_matrix(aspect);
    Mat4 view  = cam.view_matrix();
    Mat4 model = cam.model_matrix();
    Mat4 mvp   = Mat4::multiply(proj, Mat4::multiply(view, model));

    use_shader(g.shader);
    glUniformMatrix4fv(g.shader.u_mvp, 1, GL_FALSE, mvp.m);
    if (g.shader.u_model >= 0)
        glUniformMatrix4fv(g.shader.u_model, 1, GL_FALSE, model.m);
    if (g.shader.u_shading >= 0)
        glUniform1f(g.shader.u_shading, g.shading ? 1.f : 0.f);
    if (g.shader.u_light_dir >= 0) {
        // Soft key light from upper-left of the view
        glUniform3f(g.shader.u_light_dir, -0.4f, 0.5f, 0.7f);
    }

    if (g.texture.id) {
        bind_texture(g.texture, 0);
        glUniform1i(g.shader.u_tex, 0);
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    draw_mesh(g.mesh);
}
