#pragma once
#include <SDL3/SDL.h>
#include "camera.hpp"
#include "globe.hpp"
#include "countries.hpp"
#include "map_view.hpp"

struct InputState {
    bool  dragging = false;
    bool  shift_down = false;
    bool  rotation_drag = false;
    bool  trackball = false; // T: relative mouse, continuous orbit
    float last_x = 0.f;
    float last_y = 0.f;
    bool  quit = false;

    int viewport_w = 1;
    int viewport_h = 1;
    float dt = 1.f / 60.f;

    SDL_Window* window = nullptr;
};

void input_handle_event(InputState& in, Camera& cam, MapView& map,
                        Globe& globe, Countries& countries, const SDL_Event& e);
void input_set_trackball(InputState& in, bool on);
