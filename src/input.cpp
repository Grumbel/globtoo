#include "input.hpp"
#include <cstdio>

static void mouse_to_ndc(const InputState& in, float mx, float my, float& ndc_x, float& ndc_y) {
    // SDL: origin top-left, Y down. NDC: origin center, Y up.
    ndc_x = (mx / (float)in.viewport_w) * 2.f - 1.f;
    ndc_y = 1.f - (my / (float)in.viewport_h) * 2.f;
}

void input_handle_event(InputState& in, Camera& cam, Globe& globe, const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        in.quit = true;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
            in.dragging = true;
            in.last_x = e.button.x;
            in.last_y = e.button.y;
            camera_grab_kill_inertia(cam);

            if (cam.mode == ControlMode::Grab) {
                float nx, ny;
                mouse_to_ndc(in, e.button.x, e.button.y, nx, ny);
                float aspect = (float)in.viewport_w / (float)in.viewport_h;
                if (!camera_grab_begin(cam, nx, ny, aspect)) {
                    // Missed the sphere — treat as no drag
                    in.dragging = false;
                }
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

    case SDL_EVENT_MOUSE_MOTION:
        if (in.dragging) {
            float aspect = (float)in.viewport_w / (float)(in.viewport_h > 0 ? in.viewport_h : 1);
            if (cam.mode == ControlMode::Grab) {
                float nx, ny;
                mouse_to_ndc(in, e.motion.x, e.motion.y, nx, ny);
                camera_grab_move(cam, nx, ny, aspect, in.dt);
            } else {
                float dx = e.motion.x - in.last_x;
                float dy = e.motion.y - in.last_y;
                camera_apply_drag_arcball(cam, dx, dy, 0.004f);
            }
            in.last_x = e.motion.x;
            in.last_y = e.motion.y;
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        camera_apply_zoom(cam, e.wheel.y);
        break;

    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_ESCAPE) {
            in.quit = true;
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

    default:
        break;
    }
}
