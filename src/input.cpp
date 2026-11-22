#include "input.hpp"
#include <cstdio>

static void mouse_to_ndc(const InputState& in, float mx, float my, float& ndc_x, float& ndc_y) {
    ndc_x = (mx / (float)in.viewport_w) * 2.f - 1.f;
    ndc_y = 1.f - (my / (float)in.viewport_h) * 2.f;
}

void input_set_trackball(InputState& in, bool on) {
    in.trackball = on;
    in.dragging = false;
    in.rotation_drag = false;
    if (in.window) {
        // SDL3: relative mode grabs the mouse and reports deltas
        if (!SDL_SetWindowRelativeMouseMode(in.window, on)) {
            fprintf(stderr, "SDL_SetWindowRelativeMouseMode failed: %s\n", SDL_GetError());
        }
    }
    fprintf(stderr, "Trackball: %s\n", on ? "on (mouse grabbed)" : "off");
}

void input_handle_event(InputState& in, Camera& cam, MapView& map,
                        Globe& globe, Countries& countries, const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        in.quit = true;
        break;

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
        } else if (e.key.key == SDLK_N) {
            countries.names_visible = !countries.names_visible;
        } else if (e.key.key == SDLK_T && map.flat_goal < .5f) {
            input_set_trackball(in, !in.trackball);
            camera_grab_kill_inertia(cam);
            camera_grab_end(cam);
        } else if (e.key.key == SDLK_TAB) {
            if (cam.mode == ControlMode::Arcball) {
                cam.mode = ControlMode::Grab;
                fprintf(stderr, "Control: Grab (surface under cursor)\n");
            } else {
                cam.mode = ControlMode::Arcball;
                camera_grab_end(cam);
                fprintf(stderr, "Control: Arcball\n");
            }
            in.dragging = false;
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
        if (e.button.button == SDL_BUTTON_LEFT) {
            in.dragging = true;
            in.rotation_drag = in.shift_down || (SDL_GetModState() & SDL_KMOD_SHIFT);
            in.last_x = e.button.x;
            in.last_y = e.button.y;
            camera_grab_kill_inertia(cam);

            if (map.flat_goal > .5f || in.rotation_drag)
                break;

            if (cam.mode == ControlMode::Grab) {
                float nx, ny;
                mouse_to_ndc(in, e.button.x, e.button.y, nx, ny);
                float aspect = (float)in.viewport_w / (float)(in.viewport_h > 0 ? in.viewport_h : 1);
                if (!camera_grab_begin(cam, nx, ny, aspect))
                    in.dragging = false;
            }
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT) {
            in.dragging = false;
            in.rotation_drag = false;
            if (cam.mode == ControlMode::Grab)
                camera_grab_end(cam);
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
                if (in.rotation_drag) map_apply_roll(map,dx);
                else map_pan(map,dx,dy,in.viewport_h);
            }
        } else if (in.rotation_drag || in.shift_down) {
            camera_apply_roll(cam, dx, 0.005f);
        } else if (in.trackball) {
            // Continuous arcball-style orbit while mouse is grabbed
            camera_apply_drag_arcball(cam, dx, dy, 0.004f);
        } else if (cam.mode == ControlMode::Grab) {
            float aspect = (float)in.viewport_w / (float)(in.viewport_h > 0 ? in.viewport_h : 1);
            float nx, ny;
            mouse_to_ndc(in, e.motion.x, e.motion.y, nx, ny);
            camera_grab_move(cam, nx, ny, aspect, in.dt);
        } else {
            camera_apply_drag_arcball(cam, dx, dy, 0.004f);
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
