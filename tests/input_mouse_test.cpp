// Mouse navigation regression: left rotates the 2D geography, while middle
// translates the sheet without changing the geographical reference frame.
#include "input.hpp"
#include <cassert>
#include <cmath>

int main() {
    InputState in{};
    in.viewport_w=1200;
    in.viewport_h=800;
    Camera cam{};
    MapView map{};
    map.selected=map.from=map.to=MapProjection::Mercator;
    map.flat_goal=map.globe_to_flat=1.f;
    map.center_lon=.5;
    map.center_lat=.4;
    map.zoom=12.f;
    Globe globe{};
    Countries countries{};
    SDL_Event event{};

    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_MIDDLE;
    event.button.x=400;
    event.button.y=300;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(in.dragging && in.drag_button==SDL_BUTTON_MIDDLE);
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=550;
    event.motion.y=340;
    input_handle_event(in,cam,map,globe,countries,event);
    const double unit=2.*std::tan(.35)*map.map_distance/800.;
    assert(std::abs(map.pan_x-150.*unit)<1e-5);
    assert(std::abs(map.pan_y+40.*unit)<1e-5);
    assert(map.center_lon==.5 && map.center_lat==.4);
    assert(!map.navigation_active);

    // Releasing an unrelated button must not cancel the grab.
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button=SDL_BUTTON_LEFT;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(in.dragging && in.drag_button==SDL_BUTTON_MIDDLE);
    event.button.button=SDL_BUTTON_MIDDLE;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(!in.dragging && in.drag_button==0);

    // Left drag still performs great-circle navigation, not sheet movement.
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;
    event.button.x=400;
    event.button.y=300;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(in.dragging && in.drag_button==SDL_BUTTON_LEFT);
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=450;
    event.motion.y=300;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(map.navigation_active);
    assert(std::abs(map.pan_x-150.*unit)<1e-5);
    assert(std::abs(map.pan_y+40.*unit)<1e-5);

    event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button=SDL_BUTTON_LEFT;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(!in.dragging && in.drag_button==0);

    // Middle clicks on the 3D globe are ignored, not interpreted as spins.
    map.flat_goal=map.globe_to_flat=0.f;
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_MIDDLE;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(!in.dragging);
    return 0;
}
