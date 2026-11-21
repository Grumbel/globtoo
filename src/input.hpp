#pragma once
#include <SDL3/SDL.h>
#include "camera.hpp"

struct InputState {
    bool  dragging = false;
    float last_x = 0.f;
    float last_y = 0.f;
    bool  quit = false;
};

void input_handle_event(InputState& in, Camera& cam, const SDL_Event& e);
