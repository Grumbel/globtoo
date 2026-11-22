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

static SDL_Window*   window  = nullptr;
static SDL_GLContext glctx   = nullptr;

static bool init_sdl(int w, int h) {
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

    window = SDL_CreateWindow("globtoo", w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);
    return true;
}

int main(int argc, char** argv) {
    const int width  = 1280;
    const int height = 800;

    if (!init_sdl(width, height))
        return 1;

    const char* tex_path = "assets/earth.jpg";
    if (argc > 1) {
        tex_path = argv[1];
    } else if (const char* asset_dir = std::getenv("GLOBE_ASSET_DIR")) {
        static std::string path = std::string(asset_dir) + "/assets/earth.jpg";
        tex_path = path.c_str();
    }

    Globe globe{};
    if (!globe_init(globe, tex_path)) {
        fprintf(stderr, "globe_init failed\n");
        return 1;
    }

    Camera cam{};
    const auto home = timezone_center();
    camera_center_on(cam,home[0],home[1]);
    fprintf(stderr, "Startup center from timezone: %.2f E, %.2f N (or UTC-offset fallback)\n",
            home[0],home[1]);
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

    fprintf(stderr, "Controls: on-screen buttons, drag=spin/pan (cross poles in maps), Shift+drag=rotate/roll, wheel=zoom, 1..5=views, Tab=Grab/Arcball, Space=shading, B=borders, N=names, P=political, V=texture, Esc=quit\n");
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
        ui_draw_country_names(ui,countries,cam,map,aspect);
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
