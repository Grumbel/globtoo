// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// GUI visibility is independent of the camera control mode, and a hidden
// toolbar must not consume mouse input that belongs to the map.
#include "ui.hpp"
#include <cassert>
#include <cmath>

int main() {
    Ui ui{};
    ui.viewport_w = 1280;
    ui.viewport_h = 800;
    Camera cam{};
    MapView map{};
    Globe globe{};
    Countries countries{};
    InputState input{};
    input.viewport_w = ui.viewport_w;
    input.viewport_h = ui.viewport_h;

    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_TAB;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.visible);
    assert(cam.mode == ControlMode::Grab); // Tab must not change orbit mode.

    // Holding Tab must not show the controls again via key auto-repeat.
    event.key.repeat = true;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.visible);

    // With GUI hidden, pointer events should reach the map even when
    // positioned over the toolbar or compass.
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 40.f;
    event.button.y = 50.f;
    assert(!ui_event(ui,event,cam,map,globe,countries,input));
    event.button.x = 1175.f;
    event.button.y = 178.f;
    assert(!ui_event(ui,event,cam,map,globe,countries,input));
    assert(cam.mode == ControlMode::Grab);

    ui_draw(ui,cam,map,globe,countries);
    assert(ui.triangles.empty());

    // The next non-repeat Tab restores the GUI.
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_TAB;
    event.key.repeat = false;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(ui.visible);

    // Grab / Arcball now lives below Reset, inside the compass panel.
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 1175.f;
    event.button.y = 179.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(cam.mode == ControlMode::Arcball);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(cam.mode == ControlMode::Grab);

    // G is the keyboard alternative, while Tab only affects visibility.
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_G;
    input_handle_event(input,cam,map,globe,countries,event);
    assert(cam.mode == ControlMode::Arcball);
    event.key.repeat = true;
    input_handle_event(input,cam,map,globe,countries,event);
    assert(cam.mode == ControlMode::Arcball);
    event.key.repeat = false;
    input_handle_event(input,cam,map,globe,countries,event);
    assert(cam.mode == ControlMode::Grab);

    // The old top-left toolbar no longer intercepts map input.
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 525.f;
    event.button.y = 85.f;
    assert(!ui_event(ui,event,cam,map,globe,countries,input));

    // Main toolbar has moved to the bottom; hit-testing follows drawing.
    event.button.y = 731.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(ui.grid_visible);

    // New third-row overlays are independent, and can be toggled separately.
    event.button.y = 772.f;
    event.button.x = 40.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.capitals_visible && ui.cities_visible && ui.rivers_visible);
    event.button.x = 160.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.cities_visible);
    event.button.x = 260.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.rivers_visible);

    // Optional Relief layer is selectable without affecting political fills.
    // The default/absent dataset must not silently select nonexistent tiles.
    event.button.x=343.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery);
    globe.relief.preview.id=1; // simulate a validated, installed tile set
    globe.relief.cells.resize(1);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Relief);
    assert(globe.texture_visible);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery);

    // Measured elevation is independent of the photographic Relief layer.
    event.button.x=453.f;
    event.button.y=772.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery); // missing optional dataset
    globe.elevation.preview.id=1;
    globe.elevation.cells.resize(1);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Elevation);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery);

    // Ruler has moved to the right, just below the compass; it consumes clicks.
    event.button.x = 1100.f;
    event.button.y = 245.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));

    // North Lock is a separate button below Grab/Arcball.
    event.button.x = 1175.f;
    event.button.y = 207.f;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(map.north_locked);
    map.flat_goal=map.globe_to_flat=1.f;
    map.from=map.to=map.selected=MapProjection::Mercator;
    map.roll=.8;
    map_enforce_north(map,cam);
    assert(std::abs(map_north_heading(map,cam))<1e-5);
    map_pan(map,100.f,50.f,800);
    map_enforce_north(map,cam);
    assert(std::abs(map_north_heading(map,cam))<1e-5);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!map.north_locked);
    return 0;
}
