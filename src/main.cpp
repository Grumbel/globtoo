#include <SDL3/SDL.h>
#include <SDL3/SDL_opengles2.h>
#include <cstdio>
#include <chrono>

#include "globe.hpp"
#include "camera.hpp"
#include "input.hpp"

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

    window = SDL_CreateWindow("Globe", w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    glctx = SDL_GL_CreateContext(window);
    if (!glctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1); // vsync
    return true;
}

int main(int argc, char** argv) {
    const int width  = 1280;
    const int height = 800;

    if (!init_sdl(width, height))
        return 1;

    // Prefer a texture next to the binary; fall back to a solid color if missing
    const char* tex_path = "assets/earth.jpg";
    if (argc > 1)
        tex_path = argv[1];

    Globe globe{};
    if (!globe_init(globe, tex_path)) {
        fprintf(stderr, "globe_init failed\n");
        return 1;
    }

    Camera cam{};
    InputState input{};

    auto last = std::chrono::steady_clock::now();

    while (!input.quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            input_handle_event(input, cam, e);

        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - last).count();
        last = now;
        if (dt > 0.05f) dt = 0.05f; // clamp spikes

        camera_update(cam, dt);

        int w, h;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0, 0, w, h);

        glClearColor(0.02f, 0.02f, 0.05f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float aspect = (h > 0) ? (float)w / (float)h : 1.f;
        globe_draw(globe, cam, aspect);

        SDL_GL_SwapWindow(window);
    }

    globe_shutdown(globe);
    SDL_GL_DestroyContext(glctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
