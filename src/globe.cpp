// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "globe.hpp"
#include "scene.hpp"
#include "geocoordinates.hpp"
#include "resources.hpp"
#include <cstdio>
#include <filesystem>


bool globe_init(Globe& g, const char* texture_path,
                const char* relief_manifest_path,
                const char* elevation_manifest_path,
                const char* population_manifest_path,
                const char* imagery_manifest_path) {
    // Extra tessellation keeps the texture/silhouette smooth at close zoom.
    // 257 * 129 vertices still fit GLES2 16-bit element indices.
    static_assert(257 * 129 <= 65536);
    g.mesh = create_sphere(256, 128);
    g.shader = load_shader("shaders/globe.vert", "shaders/globe.frag");
    if (!g.shader.program) {
        fprintf(stderr, "Failed to load shaders\n");
        return false;
    }
    if (texture_path && g.body_textures[body_index(BodyId::Earth)].empty())
        g.body_textures[body_index(BodyId::Earth)] = texture_path;
    // Packagers may install normalized, attributed planetary mosaics without
    // requiring an explicit --body-texture for each launch. CLI always wins.
    for (const BodyInfo& body : body_catalog) {
        if (body.id == BodyId::Earth) continue;
        auto& path = g.body_textures[body_index(body.id)];
        if (path.empty()) {
            const std::string installed = resource_path(body.default_texture_resource);
            if (std::filesystem::is_regular_file(installed)) path = installed;
        }
    }
    // Each body's imagery has its own cache and preview; swapping bodies must
    // never reuse Earth's raster or an unrelated planetary mosaic.
    for (const BodyInfo& body : body_catalog) {
        const auto index=body_index(body.id);
        std::string path=g.body_imagery_manifests[index];
        if (body.id==BodyId::Earth && imagery_manifest_path)
            path=imagery_manifest_path;
        if (path.empty()) {
            path=resource_path(body.default_imagery_manifest);
        }
        raster_cache_init(g.imagery[index],path);
    }
    // Initial body loads after the tile caches, so missing-map warnings only
    // appear if neither a single texture nor tiles are available.
    globe_select_body(g, g.body);
    raster_cache_init(g.relief, relief_manifest_path ? relief_manifest_path :
                                         resource_path("relief/manifest.txt"));
    if (raster_cache_init(g.elevation, elevation_manifest_path ? elevation_manifest_path :
                                         resource_path("elevation/manifest.txt")) &&
        !g.elevation.legend.valid()) {
        fprintf(stderr,"Elevation layer missing valid measurement legend; ignoring it.\n");
        raster_cache_shutdown(g.elevation);
    }
    if (raster_cache_init(g.population, population_manifest_path ? population_manifest_path :
                                         resource_path("population/manifest.txt")) &&
        (!g.population.legend.valid() || !g.population.legend.logarithmic ||
         g.population.legend.units != "PEOPLE / KM2")) {
        fprintf(stderr,"Population layer missing valid measured-density legend; ignoring it.\n");
        raster_cache_shutdown(g.population);
    }
    if (g.body == BodyId::Earth && !g.texture.id) {
        fprintf(stderr, "%s\n", g.imagery[body_index(BodyId::Earth)].valid()
                ? "Base Earth texture unavailable; using imagery-tile preview"
                : "Failed to load earth texture – using solid color fallback");
    }
    g.shading = true;
    return true;
}

void globe_shutdown(Globe& g) {
    destroy_mesh(g.mesh);
    destroy_shader(g.shader);
    destroy_texture(g.texture);
    for (auto& imagery : g.imagery) raster_cache_shutdown(imagery);
    raster_cache_shutdown(g.relief);
    raster_cache_shutdown(g.elevation);
    raster_cache_shutdown(g.population);
}

void globe_draw(Globe& g, const Camera& cam, float aspect,
                const MapView& map) {
    RasterCache* raster = g.surface==SurfaceLayer::Imagery
                         ? &g.imagery[body_index(g.body)] :
                         g.body!=BodyId::Earth ? nullptr :
                         g.surface==SurfaceLayer::Relief ? &g.relief :
                         g.surface==SurfaceLayer::Elevation ? &g.elevation :
                         g.surface==SurfaceLayer::Population ? &g.population : nullptr;
    if (raster && !raster->valid()) raster=nullptr;
    const bool tiled = g.texture_visible && raster!=nullptr;
    const bool textured = g.texture_visible && (g.texture.id != 0 || tiled);
    Mat4 model = Mat4::identity();

    use_shader(g.shader);
    scene_uniforms(g.shader, cam, aspect, map);
    if (g.shader.u_model >= 0)
        glUniformMatrix4fv(g.shader.u_model, 1, GL_FALSE, model.m);
    if (g.shader.u_shading >= 0)
        glUniform1f(g.shader.u_shading, g.shading ? 1.f : 0.f);
    if (g.shader.u_color >= 0) {
        const auto& color=body_info(g.body).untextured_rgb;
        glUniform3f(g.shader.u_color, color[0], color[1], color[2]);
    }
    if (g.shader.u_light_dir >= 0) {
        // Headlight: rotates with the camera so the facing hemisphere stays lit
        Vec3 L = camera_headlight_dir(cam);
        glUniform3f(g.shader.u_light_dir, L.x, L.y, L.z);
    }

    const GLint u_textured = g.shader.u_textured;
    if (u_textured >= 0)
        glUniform1f(u_textured, textured ? 1.f : 0.f);

    if (textured) glUniform1i(g.shader.u_tex, 0);

    const bool sphere = map.globe_to_flat < .001f;
    const bool flat = map.globe_to_flat > .999f;
    if (flat) glDisable(GL_DEPTH_TEST);
    else { glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); }
    glDepthMask(flat ? GL_FALSE : GL_TRUE);
    if (sphere) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }
    // The whole-world texture under any tiles: the original (e.g. 4K)
    // imagery source when present, otherwise the tile set's own preview.
    const Texture* base = nullptr;
    if (textured) {
        base = tiled && (g.surface != SurfaceLayer::Imagery || !g.texture.id)
             ? &raster->preview : &g.texture;
    }
    // Stencil bit 0 marks the globe silhouette (overlays test it), bit 1
    // marks pixels already covered by a tile. Tiles are drawn finest level
    // first, so every pixel keeps the finest loaded tile, and the base
    // sphere fills whatever is still uncovered (tiles streaming, outside the
    // tiled area, gaps at a coarsely tessellated limb). Morphs deform the
    // surface so it may overlap itself; they draw the base texture only.
    const bool composite = tiled && (sphere || flat);
    if (sphere || composite) {
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_NOTEQUAL, 0x03, 0x02);
        glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    } else {
        glDisable(GL_STENCIL_TEST);
    }
    if (composite) {
        GLint viewport[4]{};
        glGetIntegerv(GL_VIEWPORT, viewport);
        RasterView view{};
        view.aspect = aspect;
        view.viewport_height = viewport[3];
        view.base_width = base ? base->width : 0;
        glStencilMask(0x03);
        raster_cache_draw(*raster, cam, map, view, g.shader);
    }
    glStencilMask(0x01);
    if (base) bind_texture(*base, 0);
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
        glStencilFunc(GL_EQUAL, 0x01, 0x01);
        glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    } else {
        glDisable(GL_STENCIL_TEST);
    }
    glDepthMask(GL_TRUE);
}
