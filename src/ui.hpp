#pragma once
#include "shader.hpp"
#include "camera.hpp"
#include "globe.hpp"
#include "input.hpp"
#include "countries.hpp"
#include "map_view.hpp"
#include <SDL3/SDL.h>
#include <string_view>
#include <vector>

struct UiVertex { float x,y,r,g,b,a; };
struct Ui {
    Shader shader{};
    GLuint vbo = 0;
    int viewport_w = 1280, viewport_h = 800;
    int hover = -1;
    std::vector<UiVertex> triangles;
};

bool ui_init(Ui& ui);
void ui_shutdown(Ui& ui);
// Return true if the event was consumed (keeps UI clicks away from globe drag).
bool ui_event(Ui& ui, const SDL_Event& event, Camera& cam, MapView& map,
              Globe& globe, Countries& countries, InputState& input);
void ui_draw(Ui& ui, const Camera& cam, const MapView& map, const Globe& globe,
             const Countries& countries);
void ui_draw_country_names(Ui& ui, const Countries& countries, const Camera& cam,
                           const MapView& map, float aspect);
void ui_flush(Ui& ui);
