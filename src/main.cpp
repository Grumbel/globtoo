// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdlib>
#include <string>
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>
#include <cstdio>
#include <chrono>

#include "globe.hpp"
#include "camera.hpp"
#include "input.hpp"
#include "stars.hpp"
#include "countries.hpp"
#include "layer_composition.hpp"
#include "ui.hpp"
#include "map_view.hpp"
#include "markers.hpp"
#include "resources.hpp"
#include "stb_image.h"

#ifndef GLOBTOO_VERSION
#error "GLOBTOO_VERSION must be provided by CMake"
#endif

static SDL_Window*   window  = nullptr;
static SDL_GLContext glctx   = nullptr;

static void set_application_icon() {
    const std::string path = resource_path("assets/globtoo-icon.png");
    int w = 0, h = 0, source_channels = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &source_channels, 4);
    if (!pixels) return; // Icon is optional for source-tree test builds.
    SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, pixels, w * 4);
    if (surface) {
        SDL_SetWindowIcon(window, surface);
        SDL_DestroySurface(surface);
    }
    stbi_image_free(pixels);
}


static bool init_sdl(int w, int h) {
    // Used by desktop environments (notably Wayland) to associate the window
    // with io.github.Grumbel.globtoo.desktop and its installed icon.
    SDL_SetAppMetadata("Globtoo", GLOBTOO_VERSION, "io.github.Grumbel.globtoo");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window = SDL_CreateWindow("globtoo " GLOBTOO_VERSION, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    resources_set_executable_dir(SDL_GetBasePath() ? SDL_GetBasePath() : "");
    set_application_icon();
    SDL_GL_SetSwapInterval(1);
    return true;
}

int main(int argc, char** argv) {
    CommandLine options;
    std::string error;
    if (!parse_command_line(argc, argv, options, error)) {
        fprintf(stderr, "globtoo: %s\nTry 'globtoo --help' for usage.\n", error.c_str());
        return 2;
    }
    if (options.action == CommandAction::Help) {
        std::fputs(command_line_help(argc ? argv[0] : "globtoo").c_str(), stdout);
        return 0;
    }
    if (options.action == CommandAction::Version) {
        std::printf("globtoo %s\n", GLOBTOO_VERSION);
        return 0;
    }

    const int width  = 1280;
    const int height = 800;

    if (!init_sdl(width, height))
        return 1;

    const std::string texture = options.texture_path.empty()
        ? resource_path("assets/earth.jpg") : options.texture_path;

    Globe globe{};
    if (!globe_init(globe, texture.c_str())) {
        fprintf(stderr, "globe_init failed\n");
        return 1;
    }

    Camera cam{};
    const auto home = options.center.value_or([&] {
        const auto tz=timezone_center();
        return std::make_pair(tz[0],tz[1]);
    }());
    camera_center_on(cam,home.first,home.second);
    fprintf(stderr, "Startup center: %.2f E, %.2f N%s\n",
            home.first,home.second,options.center ? " (command line)" : " (timezone)");
    MapView map{};
    InputState input{};
    input.viewport_w = width;
    input.viewport_h = height;
    input.window = window;

    Stars stars{};
    if (!stars_init(stars))
        fprintf(stderr, "Warning: stars failed to init\n");

    Countries countries{};
    if (!countries_init(countries, nullptr))
        fprintf(stderr, "Warning: countries failed to init\n");

    Ui ui{};
    if (!ui_init(ui)) fprintf(stderr,"Warning: on-screen controls failed to init\n");

    fprintf(stderr, "Controls: on-screen buttons, left-drag=rotate Earth/reproject, middle-drag=pan 2D / roll globe, Shift+middle-drag=roll map, Shift+left-drag=roll, wheel=zoom, 1..5=views, Tab=show/hide GUI, G=Grab/Arcball, Space=shading, B=borders, N=names, P=political, V=texture, Grid=on-screen toggle, Esc=quit\n");
    fprintf(stderr, "Control mode: Grab (camera orbits fixed Earth; stars fixed in space)\n");

    auto last = std::chrono::steady_clock::now();

    while (!input.quit) {
        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        input.viewport_w = w > 0 ? w : 1;
        input.viewport_h = h > 0 ? h : 1;

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        input.dt = dt;

        ui.viewport_w = input.viewport_w;
        ui.viewport_h = input.viewport_h;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!ui_event(ui,e,cam,map,globe,countries,input))
                input_handle_event(input, cam, map, globe, countries, e);
        }

        if (map.flat_goal < .5f) camera_update(cam, dt);
        map_update(map,dt);
        map_enforce_north(map,cam);

        glViewport(0, 0, input.viewport_w, input.viewport_h);
        glClearColor(0.02f, 0.02f, 0.05f, 1.f);
        // Globe draws the visible-disc stencil mask. Reset its writable mask
        // before clearing, since countries render with stencil writes off.
        glStencilMask(0xff);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        float aspect = (float)input.viewport_w / (float)input.viewport_h;
        stars_draw(stars, cam, aspect);
        globe_draw(globe, cam, aspect, map);
        // Preserve globe depth: it occludes political geometry behind the horizon.
        countries_draw(countries, cam, aspect, map, globe.shading,
                       country_fill_opacity(globe.texture_visible, globe.texture.id != 0));
        ui_draw_graticule(ui,cam,map,aspect);
        ui_draw_country_names(ui,countries,cam,map,aspect);
        ui_draw_markers(ui,options.markers,cam,map,aspect);
        ui_draw(ui,cam,map,globe,countries);
        ui_flush(ui);

        SDL_GL_SwapWindow(window);
    }

    ui_shutdown(ui);
    countries_shutdown(countries);
    stars_shutdown(stars);
    globe_shutdown(globe);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
