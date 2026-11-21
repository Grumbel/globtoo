#include "input.hpp"
#include <cstdio>

static void mouse_to_ndc(const InputState& in, float mx, float my, float& ndc_x, float& ndc_y) {
    ndc_x = (mx / (float)in.viewport_w) * 2.f - 1.f;
    ndc_y = 1.f - (my / (float)in.viewport_h) * 2.f;
}

void input_set_trackball(InputState& in, bool on) {
    in.trackball = on;
    in.dragging = false;
    if (in.window) {
        // SDL3: relative mode grabs the mouse and reports deltas
        if (!SDL_SetWindowRelativeMouseMode(in.window, on)) {
            fprintf(stderr, "SDL_SetWindowRelativeMouseMode failed: %s\n", SDL_GetError());
        }
    }
    fprintf(stderr, "Trackball: %s\n", on ? "on (mouse grabbed)" : "off");
}

void input_handle_event(InputState& in, Camera& cam, Globe& globe, const SDL_Event& e) {
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
        } else if (e.key.key == SDLK_T) {
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
        }
        break;

    case SDL_EVENT_KEY_UP:
        if (e.key.key == SDLK_LSHIFT || e.key.key == SDLK_RSHIFT)
            in.shift_down = false;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (in.trackball)
            break; // motion-only in trackball
        if (e.button.button == SDL_BUTTON_LEFT) {
            in.dragging = true;
            in.last_x = e.button.x;
            in.last_y = e.button.y;
            camera_grab_kill_inertia(cam);

            if (in.shift_down)
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

        if (in.shift_down) {
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
        camera_apply_zoom(cam, e.wheel.y);
        break;

    default:
        break;
    }
}
