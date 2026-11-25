// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <SDL3/SDL.h>
#include <vector>
#include "camera.hpp"
#include "globe.hpp"
#include "countries.hpp"
#include "map_view.hpp"

struct InputState {
    bool  dragging = false;
    int   drag_button = 0; // left: geographic rotation, middle: 2D sheet grab / globe roll
    SDL_MouseID drag_mouse = 0;
    Uint64 grab_motion_ns = 0;
    std::vector<SDL_TouchFingerEvent> touch_fingers;
    bool touch_pinching = false;
    float x11_pinch_scale = 0.f;

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
void input_update_gamepad(const InputState& in, Camera& cam, MapView& map, SDL_Gamepad* pad);
