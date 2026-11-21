#pragma once
#include <SDL3/SDL.h>
#include "camera.hpp"
#include "globe.hpp"

struct InputState {
    bool  dragging = false;
    float last_x = 0.f;
    float last_y = 0.f;
    bool  quit = false;

    int viewport_w = 1;
    int viewport_h = 1;
    float dt = 1.f / 60.f;
};

void input_handle_event(InputState& in, Camera& cam, Globe& globe, const SDL_Event& e);
