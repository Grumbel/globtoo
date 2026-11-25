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

    // A second finger takes over from the emulated single-finger drag.
    const auto touch = [&](SDL_EventType type, SDL_FingerID id, float x, float y = .5f) {
        SDL_Event finger{};
        finger.tfinger.type = type;
        finger.tfinger.touchID = 1;
        finger.tfinger.fingerID = id;
        finger.tfinger.x = x;
        finger.tfinger.y = y;
        input_handle_event(in, cam, map, globe, countries, finger);
    };
    touch(SDL_EVENT_FINGER_DOWN, 1, .375f);
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.which = SDL_TOUCH_MOUSEID;
    event.button.x = 450;
    event.button.y = 400;
    input_handle_event(in, cam, map, globe, countries, event);
    assert(in.dragging);
    cam.distance = 3.f;
    touch(SDL_EVENT_FINGER_DOWN, 2, .5f);
    assert(!in.dragging && in.touch_pinching && cam.distance == 3.f);
    touch(SDL_EVENT_FINGER_MOTION, 2, .625f);
    assert(cam.distance == 2.f);
    touch(SDL_EVENT_FINGER_MOTION, 2, .5f);
    assert(cam.distance == 3.f);
    touch(SDL_EVENT_FINGER_UP, 2, .5f);
    assert(in.touch_pinching);
    touch(SDL_EVENT_FINGER_MOTION, 1, .25f);
    assert(cam.distance == 3.f);
    touch(SDL_EVENT_FINGER_UP, 1, .25f);
    assert(!in.touch_pinching);

    // A clockwise quarter turn at constant pixel radius rolls the globe
    // by the same angle even in a non-square viewport.
    cam.orientation = {0.f, 0.f, 0.f, 1.f};
    touch(SDL_EVENT_FINGER_DOWN, 1, .375f);
    touch(SDL_EVENT_FINGER_DOWN, 2, .5f);
    touch(SDL_EVENT_FINGER_MOTION, 2, .375f, .6875f);
    assert(cam.distance == 3.f);
    assert(std::abs(cam.orientation.z - std::sqrt(.5f)) < 1e-6f);
    assert(std::abs(cam.orientation.w - std::sqrt(.5f)) < 1e-6f);
    assert(cam.ang_vel.z == 0.f);

    // A twist can also change zoom in the same motion.
    touch(SDL_EVENT_FINGER_MOTION, 2, .625f);
    assert(cam.distance == 2.f);
    assert(std::abs(cam.orientation.z) < 1e-6f);
    map.north_locked = true;
    touch(SDL_EVENT_FINGER_MOTION, 2, .375f, .6875f);
    assert(cam.distance == 3.f);
    assert(std::abs(cam.orientation.z) < 1e-6f);
    map.north_locked = false;
    touch(SDL_EVENT_FINGER_UP, 1, .375f);
    touch(SDL_EVENT_FINGER_UP, 2, .375f, .6875f);

    // Flat zoom preserves the center of a translated sheet.
    map.flat_goal = 1.f;
    map.zoom = 4.f;
    map.pan_x = .5;
    map.pan_y = -.25;
    touch(SDL_EVENT_FINGER_DOWN, 1, .375f);
    touch(SDL_EVENT_FINGER_DOWN, 2, .5f);
    touch(SDL_EVENT_FINGER_MOTION, 2, .625f);
    assert(map.zoom == 8.f && map.pan_x == 1. && map.pan_y == -.5);
    touch(SDL_EVENT_FINGER_MOTION, 2, .5f);
    assert(map.zoom == 4.f && map.pan_x == .5 && map.pan_y == -.25);

    // A clockwise twist rolls the translated sheet and can zoom at the same time.
    map.roll = 0.;
    touch(SDL_EVENT_FINGER_MOTION, 2, .375f, .875f);
    assert(map.zoom == 8.f);
    assert(std::abs(map.roll + std::acos(-1.) / 2.) < 1e-6);
    assert(std::abs(map.pan_x + .5) < 1e-6 && std::abs(map.pan_y + 1.) < 1e-6);
    assert(std::abs(cam.orientation.z) < 1e-6f);

    map.north_locked = true;
    const double locked_touch_roll = map.roll;
    touch(SDL_EVENT_FINGER_MOTION, 2, .5f);
    assert(map.zoom == 4.f && map.roll == locked_touch_roll);
    map.north_locked = false;
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    input_handle_event(in, cam, map, globe, countries, event);
    assert(in.touch_fingers.empty() && !in.touch_pinching);

#if SDL_VERSION_ATLEAST(3, 4, 0) && !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
    // SDL trackpad events carry a scale factor rather than finger positions.
    SDL_Event pinch{};
    pinch.type = SDL_EVENT_PINCH_BEGIN;
    input_handle_event(in, cam, map, globe, countries, pinch);
    pinch.type = SDL_EVENT_PINCH_UPDATE;
    pinch.pinch.scale = 2.f;
    input_handle_event(in, cam, map, globe, countries, pinch);
    assert(map.zoom == 8.f);
    pinch.pinch.scale = .5f;
    input_handle_event(in, cam, map, globe, countries, pinch);
    assert(map.zoom == 4.f);
    pinch.type = SDL_EVENT_PINCH_END;
    input_handle_event(in, cam, map, globe, countries, pinch);
#endif

    const auto pointer = [&](SDL_EventType type, float x, float y, Uint64 ms) {
        SDL_Event mouse{};
        if (type == SDL_EVENT_MOUSE_MOTION) {
            mouse.motion.type = type;
            mouse.motion.timestamp = ms * SDL_NS_PER_MS;
            mouse.motion.which = SDL_TOUCH_MOUSEID;
            mouse.motion.x = x;
            mouse.motion.y = y;
        } else {
            mouse.button.type = type;
            mouse.button.timestamp = ms * SDL_NS_PER_MS;
            mouse.button.which = SDL_TOUCH_MOUSEID;
            mouse.button.button = SDL_BUTTON_LEFT;
            mouse.button.x = x;
            mouse.button.y = y;
        }
        input_handle_event(in, cam, map, globe, countries, mouse);
    };

    map = {};
    for (float distance : {1.15f, 3.2f, 7.f}) {
        cam = {};
        cam.mode = ControlMode::Arcball;
        cam.distance = distance;
        pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 600, 400, 10);
        pointer(SDL_EVENT_MOUSE_MOTION, 620, 400, 20);
        // Project the world point that was at the viewport center.
        const float angle = -2.f * std::atan2(cam.orientation.y, cam.orientation.w);
        const float pixels = 400.f * std::sin(angle) /
            ((distance - std::cos(angle)) * std::tan(Camera::vertical_fov * .5f));
        assert(std::abs(pixels - 20.f) < .1f);
        pointer(SDL_EVENT_MOUSE_BUTTON_UP, 620, 400, 21);
    }

    cam = {};
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 600, 400, 50);
    pointer(SDL_EVENT_MOUSE_MOTION, 680, 400, 70);
    const float grab_angle = -2.f * std::atan2(cam.orientation.y, cam.orientation.w);
    const float grab_pixels = 400.f * std::sin(grab_angle) /
        ((cam.distance - std::cos(grab_angle)) * std::tan(Camera::vertical_fov * .5f));
    assert(std::abs(grab_pixels - 80.f) < .01f);
    const float grab_y = cam.orientation.y;
    camera_update(cam, .016f);
    assert(cam.orientation.y == grab_y);
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 680, 400, 72);
    assert(std::abs(cam.ang_vel.y + grab_angle / .020f) < 1e-4f);

    cam = {};
    map.north_locked = true;
    camera_center_on(cam, 0., 40.);
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 600, 400, 100);
    for (unsigned step = 1; step <= 3; ++step) {
        const Vec3 before = camera_view_to_world(cam, {0, 0, 1});
        pointer(SDL_EVENT_MOUSE_MOTION, 600 + 30 * step, 400 + 10 * step, 100 + 10 * step);
        camera_update(cam, .016f);
        map_enforce_north(map, cam);
        assert(cam.grab_active);
        assert(std::abs(map_north_heading(map, cam)) < 1e-5);
        const Vec3 after = camera_view_to_world(cam, {0, 0, 1});
        assert(Mat4::dot(before, after) < .999f);
    }
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 690, 430, 134);
    assert(!cam.grab_active && Mat4::dot(cam.ang_vel, cam.ang_vel) > 16.f);
    const Vec3 before_release = camera_view_to_world(cam, {0, 0, 1});
    camera_update(cam, .016f);
    map_enforce_north(map, cam);
    assert(Mat4::dot(before_release, camera_view_to_world(cam, {0, 0, 1})) < .999f);
    assert(Mat4::dot(cam.ang_vel, cam.ang_vel) > 0.f);

    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN, 600, 400, 200);
    pointer(SDL_EVENT_MOUSE_MOTION, 650, 400, 210);
    pointer(SDL_EVENT_MOUSE_BUTTON_UP, 650, 400, 320);
    assert(Mat4::dot(cam.ang_vel, cam.ang_vel) == 0.f);

    // Body cycling is a real input action, not a test-only stub. The default
    // test globe has no GPU texture so this path needs no GL context.
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.key=SDLK_M;
    event.key.repeat=false;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(globe.body==BodyId::Moon);
    // Shift+M walks the catalog backwards (and wraps at the start).
    in.shift_down=true;
    input_handle_event(in,cam,map,globe,countries,event);
    assert(globe.body==BodyId::Earth);
    input_handle_event(in,cam,map,globe,countries,event);
    assert(globe.body==BodyId::Venus);
    input_handle_event(in,cam,map,globe,countries,event);
    input_handle_event(in,cam,map,globe,countries,event);
    assert(globe.body==BodyId::Charon);
    in.shift_down=false;
    return 0;
}
