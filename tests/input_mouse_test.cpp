// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

    // Shift+middle rolls a flat map; ordinary middle still pans the sheet.
    map.flat_goal=map.globe_to_flat=1.f;
    const double pan_before=std::hypot(map.pan_x,map.pan_y);
    const double roll_before=map.roll;
    in.shift_down=true;
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_MIDDLE;
    event.button.x=400;
    event.button.y=300;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(in.dragging && in.rotation_drag);
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=445;
    event.motion.y=300;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(std::abs(map.roll-roll_before)>.1);
    assert(std::abs(std::hypot(map.pan_x,map.pan_y)-pan_before)<1e-6);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button=SDL_BUTTON_MIDDLE;
    input_handle_event(in,cam,map,globe,countries,event);
    in.shift_down=false;

    // North Lock blocks manual roll without disabling ordinary sheet grab.
    map.north_locked=true;
    const double locked_roll=map.roll;
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_MIDDLE;
    event.button.x=400;
    event.button.y=300;
    in.shift_down=true;
    input_handle_event(in,cam,map,globe,countries,event);
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=470;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(map.roll==locked_roll);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button=SDL_BUTTON_MIDDLE;
    input_handle_event(in,cam,map,globe,countries,event);
    in.shift_down=false;

    // Middle-drag on the globe is a roll, equivalent to Shift+left-drag.
    map.flat_goal=map.globe_to_flat=0.f;
    map.north_locked=false;
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_MIDDLE;
    event.button.x=400;
    event.button.y=300;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(in.dragging && in.rotation_drag);
    const Quat old_cam=cam.orientation;
    event.type=SDL_EVENT_MOUSE_MOTION;
    event.motion.x=450;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(std::abs(cam.orientation.x-old_cam.x)+
           std::abs(cam.orientation.y-old_cam.y)+
           std::abs(cam.orientation.z-old_cam.z)+
           std::abs(cam.orientation.w-old_cam.w)>1e-4f);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button=SDL_BUTTON_MIDDLE;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(!in.dragging);

    // Body cycling is a real input action, not a test-only stub. The default
    // test globe has no GPU texture so this path needs no GL context.
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.key=SDLK_M;
    event.key.repeat=false;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(globe.body==BodyId::Moon);
    return 0;
}
