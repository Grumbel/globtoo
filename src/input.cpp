#include "input.hpp"

void input_handle_event(InputState& in, Camera& cam, const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_QUIT:
        in.quit = true;
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
            in.dragging = true;
            in.last_x = e.button.x;
            in.last_y = e.button.y;
            // kill residual velocity on grab – feels like catching a spinning globe
            cam.yaw_vel = 0.f;
            cam.pitch_vel = 0.f;
        }
        break;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (e.button.button == SDL_BUTTON_LEFT)
            in.dragging = false;
        break;

    case SDL_EVENT_MOUSE_MOTION:
        if (in.dragging) {
            float dx = e.motion.x - in.last_x;
            float dy = e.motion.y - in.last_y;
            // sensitivity tuned for tactile feel
            camera_apply_drag(cam, dx, dy, 0.0045f);
            in.last_x = e.motion.x;
            in.last_y = e.motion.y;
        }
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        camera_apply_zoom(cam, e.wheel.y);
        break;

    case SDL_EVENT_KEY_DOWN:
        if (e.key.key == SDLK_ESCAPE)
            in.quit = true;
        break;

    default:
        break;
    }
}
