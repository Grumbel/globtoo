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

    fprintf(stderr, "Controls: drag=spin, wheel=zoom, Tab=Grab/Arcball, T=trackball, Shift=roll, Space=shading, B=borders, P=political, Esc=quit\n");
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

        SDL_Event e;
        while (SDL_PollEvent(&e))
            input_handle_event(input, cam, globe, countries, e);

        camera_update(cam, dt);

        glViewport(0, 0, input.viewport_w, input.viewport_h);
        glClearColor(0.02f, 0.02f, 0.05f, 1.f);
        // Globe draws the visible-disc stencil mask. Reset its writable mask
        // before clearing, since countries render with stencil writes off.
        glStencilMask(0xff);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        float aspect = (float)input.viewport_w / (float)input.viewport_h;
        stars_draw(stars, cam, aspect);
        globe_draw(globe, cam, aspect, !countries.fill_visible);
        // Preserve globe depth: it occludes political geometry behind the horizon.
        countries_draw(countries, cam, aspect);

        SDL_GL_SwapWindow(window);
    }

    countries_shutdown(countries);
    stars_shutdown(stars);
    globe_shutdown(globe);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
