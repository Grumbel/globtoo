// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

static void mouse_to_ndc(const InputState& in, float mx, float my, float& ndc_x, float& ndc_y) {
    ndc_x = (mx / (float)in.viewport_w) * 2.f - 1.f;
    ndc_y = 1.f - (my / (float)in.viewport_h) * 2.f;
}

void input_set_trackball(InputState& in, bool on) {
    in.trackball = on;
    in.dragging = false;
    in.drag_button = 0;
    in.rotation_drag = false;
    if (in.window) {
        // SDL3: relative mode grabs the mouse and reports deltas
        if (!SDL_SetWindowRelativeMouseMode(in.window, on)) {
            fprintf(stderr, "SDL_SetWindowRelativeMouseMode failed: %s\n", SDL_GetError());
        }
    }
    fprintf(stderr, "Trackball: %s\n", on ? "on (mouse grabbed)" : "off");
}

static void pinch_zoom(Camera& cam, MapView& map, float scale) {
    if (!std::isfinite(scale) || scale <= 0.f || scale == 1.f) return;
    if (map.flat_goal > .5f) {
        const float zoom = std::clamp(map.zoom * scale, .6f, MapView::max_zoom);
        const double ratio = static_cast<double>(zoom) / map.zoom;
        map.pan_x *= ratio;
        map.pan_y *= ratio;
        map.zoom = zoom;
    } else {
        // Scale the distance above the globe's surface for useful zoom close in.
        cam.distance = std::clamp(1.f + (cam.distance - 1.f) / scale,
                                  cam.min_dist, cam.max_dist);
    }
}

static void end_touch_drag(InputState& in, Camera& cam) {
    if (!in.dragging || in.drag_mouse != SDL_TOUCH_MOUSEID) return;
    in.dragging = false;
    in.drag_button = 0;
    in.rotation_drag = false;
    camera_grab_end(cam);
}

static void handle_touch(InputState& in, Camera& cam, MapView& map,
                         const SDL_TouchFingerEvent& event) {
    if (event.touchID == SDL_MOUSE_TOUCHID) return;

    auto& fingers = in.touch_fingers;
    if (!fingers.empty() && event.touchID != fingers.front().touchID) return;

    auto finger = std::find_if(fingers.begin(), fingers.end(), [&](const auto& f) {
        return f.fingerID == event.fingerID;
    });
    if (event.type == SDL_EVENT_FINGER_DOWN) {
        if (finger == fingers.end()) fingers.push_back(event);
        if (fingers.size() == 2) {
            in.touch_pinching = true;
            end_touch_drag(in, cam);
            camera_grab_kill_inertia(cam);
        }
    } else if (finger != fingers.end() && event.type == SDL_EVENT_FINGER_MOTION) {
        if (fingers.size() == 2) {
            const auto separation = [&] {
                return SDL_FPoint{(fingers[1].x - fingers[0].x) * in.viewport_w,
                                  (fingers[1].y - fingers[0].y) * in.viewport_h};
            };
            const auto before = separation();
            *finger = event;
            const auto after = separation();
            const float before_span = std::hypot(before.x, before.y);
            const float after_span = std::hypot(after.x, after.y);
            // Ignore unstable scale and angle when the fingers nearly meet.
            if (before_span >= 2.f && after_span >= 2.f) {
                pinch_zoom(cam, map, after_span / before_span);
                if (!map.north_locked) {
                    const float cross = before.x * after.y - before.y * after.x;
                    const float dot = before.x * after.x + before.y * after.y;
                    // Screen Y grows down, so reverse the screen-space angle.
                    const float angle = -std::atan2(cross, dot);
                    if (map.flat_goal > .5f) {
                        map_apply_roll(map, angle, 1.f);
                    } else {
                        camera_apply_roll(cam, angle, 1.f);
                        camera_grab_kill_inertia(cam);
                    }
                }
            }
        } else {
            *finger = event;
        }
    } else if (finger != fingers.end()) {
        fingers.erase(finger);
        if (fingers.empty()) in.touch_pinching = false;
    }
}

void input_handle_event(InputState& in, Camera& cam, MapView& map,
                        Globe& globe, Countries& countries, const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        in.quit = true;
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        end_touch_drag(in, cam);
        in.touch_fingers.clear();
        in.touch_pinching = false;
        in.x11_pinch_scale = 0.f;
        break;

    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
#if SDL_VERSION_ATLEAST(3, 4, 0)
    case SDL_EVENT_FINGER_CANCELED:
#endif
        handle_touch(in, cam, map, e.tfinger);
        break;

#if SDL_VERSION_ATLEAST(3, 4, 0) && !defined(SDL_PLATFORM_ANDROID) && !defined(SDL_PLATFORM_IOS)
    // Desktop trackpads deliver SDL gestures. Android and iOS also send
    // native pinch events for the touchscreen contacts handled above.
    case SDL_EVENT_PINCH_BEGIN: {
        const char* driver = SDL_GetCurrentVideoDriver();
        // SDL forwards XInput's cumulative scale on X11.
        in.x11_pinch_scale = driver && SDL_strcmp(driver, "x11") == 0 ? 1.f : 0.f;
        camera_grab_kill_inertia(cam);
        break;
    }
    case SDL_EVENT_PINCH_UPDATE: {
        const float scale = e.pinch.scale;
        if (!std::isfinite(scale) || scale <= 0.f) break;
        if (in.x11_pinch_scale > 0.f) {
            pinch_zoom(cam, map, scale / in.x11_pinch_scale);
            in.x11_pinch_scale = scale;
        } else {
            pinch_zoom(cam, map, scale);
        }
        break;
    }
    case SDL_EVENT_PINCH_END:
        in.x11_pinch_scale = 0.f;
        break;
#endif

    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_ESCAPE) {
            if (in.trackball) {
                input_set_trackball(in, false);
                camera_grab_end(cam);
            } else {
                in.quit = true;
            }
        } else if (e.key.key == SDLK_LSHIFT || e.key.key == SDLK_RSHIFT) {
            in.shift_down = true;
        } else if (e.key.key == SDLK_1) {
            input_set_trackball(in,false);
            map_select(map,MapProjection::Globe,cam);
        } else if (e.key.key == SDLK_2) {
            input_set_trackball(in,false);
            map_select(map,MapProjection::Equirectangular,cam);
        } else if (e.key.key == SDLK_3) {
            input_set_trackball(in,false);
            map_select(map,MapProjection::Mercator,cam);
        } else if (e.key.key == SDLK_4) {
            input_set_trackball(in,false);
            map_select(map,MapProjection::Mollweide,cam);
        } else if (e.key.key == SDLK_5) {
            input_set_trackball(in,false);
            map_select(map,MapProjection::Hammer,cam);
        } else if (e.key.key == SDLK_M && !e.key.repeat) {
            const bool back = in.shift_down || (SDL_GetModState() & SDL_KMOD_SHIFT);
            globe_select_body(globe,back ? previous_body(globe.body) : next_body(globe.body));
        } else if (e.key.key == SDLK_N) {
            countries.names_visible = !countries.names_visible;
        } else if (e.key.key == SDLK_T && map.flat_goal < .5f) {
            input_set_trackball(in, !in.trackball);
            camera_grab_kill_inertia(cam);
            camera_grab_end(cam);
        } else if (e.key.key == SDLK_G && !e.key.repeat) {
            if (cam.mode == ControlMode::Arcball) {
                cam.mode = ControlMode::Grab;
                fprintf(stderr, "Control: Grab (surface under cursor)\n");
            } else {
                cam.mode = ControlMode::Arcball;
                camera_grab_end(cam);
                fprintf(stderr, "Control: Arcball\n");
            }
            in.dragging = false;
            in.drag_button = 0;
            camera_grab_kill_inertia(cam);
        } else if (e.key.key == SDLK_SPACE) {
            globe.shading = !globe.shading;
            fprintf(stderr, "Shading: %s\n", globe.shading ? "on" : "off");
        } else if (e.key.key == SDLK_B) {
            countries.borders_visible = !countries.borders_visible;
            fprintf(stderr, "Borders: %s\n", countries.borders_visible ? "on" : "off");
        } else if (e.key.key == SDLK_V) {
            globe.texture_visible = !globe.texture_visible;
            fprintf(stderr, "Texture: %s\n", globe.texture_visible ? "on" : "off");
        } else if (e.key.key == SDLK_P) {
            countries.fill_visible = !countries.fill_visible;
            fprintf(stderr, "Political fill: %s\n", countries.fill_visible ? "on" : "off");
        }
        break;

    case SDL_EVENT_KEY_UP:
        if (e.key.key == SDLK_LSHIFT || e.key.key == SDLK_RSHIFT)
            in.shift_down = false;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (in.trackball && map.flat_goal < .5f) {
            // Exit relative mode with a click so the GUI is reachable without
            // having to remember Esc or T.
            if (e.button.button == SDL_BUTTON_LEFT) input_set_trackball(in, false);
            break;
        }
        if (in.dragging) break;
        if (e.button.button == SDL_BUTTON_MIDDLE) {
            in.dragging = true;
            in.drag_button = SDL_BUTTON_MIDDLE;
            in.drag_mouse = e.button.which;
            // Keep middle-drag grab/pan in flat maps. Shift+middle-drag
            // rotates there; on the globe middle-drag is a roll gesture.
            in.rotation_drag = map.flat_goal < .5f || in.shift_down ||
                               (SDL_GetModState() & SDL_KMOD_SHIFT);
            in.last_x = e.button.x;
            in.last_y = e.button.y;
            camera_grab_kill_inertia(cam);
            break;
        }
        if (e.button.button == SDL_BUTTON_LEFT) {
            in.dragging = true;
            in.drag_button = SDL_BUTTON_LEFT;
            in.drag_mouse = e.button.which;
            in.rotation_drag = in.shift_down || (SDL_GetModState() & SDL_KMOD_SHIFT);
            in.last_x = e.button.x;
            in.last_y = e.button.y;
            camera_grab_kill_inertia(cam);

            if (map.flat_goal > .5f || in.rotation_drag)
                break;

            if (cam.mode == ControlMode::Grab) {
                in.grab_motion_ns = e.button.timestamp;
                float nx, ny;
                mouse_to_ndc(in, e.button.x, e.button.y, nx, ny);
                float aspect = (float)in.viewport_w / (float)(in.viewport_h > 0 ? in.viewport_h : 1);
                if (!camera_grab_begin(cam, nx, ny, aspect)) {
                    in.dragging = false;
                    in.drag_button = 0;
                }
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (in.dragging && e.button.button == in.drag_button) {
            in.dragging = false;
            in.drag_button = 0;
            in.rotation_drag = false;
            if (cam.grab_active) {
                // A pause before lifting indicates that the user stopped the globe.
                if (e.button.timestamp > in.grab_motion_ns &&
                    e.button.timestamp - in.grab_motion_ns >= 80 * SDL_NS_PER_MS)
                    camera_grab_kill_inertia(cam);
                camera_grab_end(cam);
            }
        }
        break;

    case SDL_EVENT_MOUSE_MOTION: {
        float dx = 0.f, dy = 0.f;
        if (in.trackball) {
            // Relative deltas when grabbed
            dx = e.motion.xrel;
            dy = e.motion.yrel;
        } else if (in.dragging) {
            dx = e.motion.x - in.last_x;
            dy = e.motion.y - in.last_y;
            in.last_x = e.motion.x;
            in.last_y = e.motion.y;
        } else {
            break;
        }

        if (map.flat_goal > .5f) {
            if (in.dragging) {
                if (in.rotation_drag) {
                    if (!map.north_locked) map_apply_roll(map,dx);
                } else if (in.drag_button == SDL_BUTTON_MIDDLE) {
                    map_grab_pan(map,dx,dy,in.viewport_h);
                } else map_pan(map,dx,dy,in.viewport_h);
            }
        } else if (in.rotation_drag || in.shift_down) {
            if (!map.north_locked) camera_apply_roll(cam, dx, 0.005f);
        } else if (in.trackball) {
            // Continuous arcball-style orbit while mouse is grabbed
            camera_apply_drag_arcball(cam, dx, dy, 0.004f);
        } else if (cam.mode == ControlMode::Grab) {
            float aspect = (float)in.viewport_w / (float)(in.viewport_h > 0 ? in.viewport_h : 1);
            float nx, ny;
            mouse_to_ndc(in, e.motion.x, e.motion.y, nx, ny);
            if (dx == 0.f && dy == 0.f) break;
            const Uint64 now = e.motion.timestamp;
            const float elapsed = now > in.grab_motion_ns
                ? static_cast<float>(now - in.grab_motion_ns) / SDL_NS_PER_SECOND : in.dt;
            in.grab_motion_ns = now;
            if (!camera_grab_move(cam, nx, ny, aspect, elapsed)) {
                camera_grab_kill_inertia(cam);
            } else if (map.north_locked) {
                map_enforce_north(map, cam);
                // Re-anchor at the pointer after rolling north-up so the next
                // motion starts from the surface position currently under it.
                camera_grab_reanchor(cam, nx, ny, aspect);
            }
        } else {
            // Match the projected motion of a surface point at screen center.
            const float sensitivity = 2.f * std::tan(Camera::vertical_fov * .5f) * (cam.distance - 1.f) / in.viewport_h;
            camera_apply_drag_arcball(cam, dx, dy, sensitivity);
        }
        break;
    }

    case SDL_EVENT_MOUSE_WHEEL:
        if (map.flat_goal > .5f) map_zoom(map,e.wheel.y);
        else camera_apply_zoom(cam, e.wheel.y);
        break;

    default:
        break;
    }
}

void input_update_gamepad(const InputState& in, Camera& cam, MapView& map, SDL_Gamepad* pad) {
    if (in.dragging) return;

    const float x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32768.f;
    const float y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32768.f;
    const float length = std::hypot(x, y);
    constexpr float deadzone = .18f;
    if (length > deadzone) {
        const float strength = (std::min(length, 1.f) - deadzone) / (1.f - deadzone);
        const float step = 360.f * strength * in.dt / length;
        if (map.flat_goal > .5f) {
            map_pan(map, x * step, y * step, in.viewport_h);
        } else {
            camera_apply_drag_arcball(cam, x * step, y * step, .004f);
            // The stick supplies velocity directly each frame.
            camera_grab_kill_inertia(cam);
        }
    }

    const auto trigger = [pad](SDL_GamepadAxis axis) {
        constexpr float deadzone = .08f;
        const float value = SDL_GetGamepadAxis(pad, axis) / 32767.f;
        return std::max(0.f, (value - deadzone) / (1.f - deadzone));
    };
    const float zoom = (trigger(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) -
                        trigger(SDL_GAMEPAD_AXIS_LEFT_TRIGGER)) * 16.f * in.dt;
    if (zoom != 0.f) {
        if (map.flat_goal > .5f) map_zoom(map, zoom);
        else camera_apply_zoom(cam, zoom);
    }
}
