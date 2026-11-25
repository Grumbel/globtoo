// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// GUI visibility is independent of the camera control mode, and a hidden
// toolbar must not consume mouse input that belongs to the map.
#include "ui.hpp"
#include <cassert>
#include <cmath>

namespace {
// Press the left button at the center of the button for `command`.
void click_at(SDL_Event& event, const Ui& ui, BodyId body, UiCommand command) {
    float x=0,y=0;
    const bool shown=ui_command_center(ui,body,command,x,y);
    assert(shown);
    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
}
bool inside(const float (&r)[4], float x, float y) {
    return x>=r[0] && x<r[0]+r[2] && y>=r[1] && y<r[1]+r[3];
}
bool overlap(const float (&a)[4], const float (&b)[4]) {
    return a[0]<b[0]+b[2] && a[0]+a[2]>b[0] && a[1]<b[1]+b[3] && a[1]+a[3]>b[1];
}
bool within(const float (&r)[4], int w, int h) {
    return r[0]>=0 && r[1]>=0 && r[0]+r[2]<=w && r[1]+r[3]<=h;
}
} // namespace

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
    click_at(event,ui,globe.body,UiCommand::Grid);
    assert(!ui_event(ui,event,cam,map,globe,countries,input));
    click_at(event,ui,globe.body,UiCommand::ControlMode);
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

    // All controls are anchored to the bottom edge: toolbar on the left,
    // compass on the right, scale bar in between; none of them overlap.
    {
        const auto p=ui_panels(ui);
        assert(p.toolbar[0]<p.scale[0] && p.scale[0]<p.compass[0]);
        assert(p.toolbar[1]+p.toolbar[3]==p.compass[1]+p.compass[3]);
        assert(p.scale[1]+p.scale[3]==p.compass[1]+p.compass[3]);
        assert(p.toolbar[1]+p.toolbar[3]>ui.viewport_h-16);
        assert(!overlap(p.toolbar,p.scale) && !overlap(p.scale,p.compass) &&
               !overlap(p.toolbar,p.compass));
        assert(p.legend[1]<100 && !overlap(p.legend,p.compass));
        // A narrow window stacks the compass and scale above the toolbar.
        Ui narrow=ui;
        narrow.viewport_w=800;
        narrow.viewport_h=600;
        const auto n=ui_panels(narrow);
        assert(!overlap(n.toolbar,n.compass) && !overlap(n.toolbar,n.scale) &&
               !overlap(n.scale,n.compass));
        assert(within(n.toolbar,800,600) && within(n.compass,800,600) &&
               within(n.scale,800,600));
    }

    // Grab / Arcball lives in the compass panel.
    click_at(event,ui,globe.body,UiCommand::ControlMode);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(cam.mode == ControlMode::Arcball);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(cam.mode == ControlMode::Grab);

    // Keyboard and compass-panel body switching must link the *real*
    // globe_select_body implementation and update the selected body.
    globe.surface=SurfaceLayer::Relief;
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.key=SDLK_M;
    event.key.repeat=false;
    input_handle_event(input,cam,map,globe,countries,event);
    assert(globe.body==BodyId::Moon);
    assert(globe.surface==SurfaceLayer::Imagery);
    // The < BODY > selector sits in the toolbar header.
    click_at(event,ui,globe.body,UiCommand::NextBody);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.body==BodyId::Mars);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.body==BodyId::Jupiter);
    // Gas giants have no named surface features, so no LANDMARKS button.
    {
        float lx=0,ly=0;
        assert(!ui_command_center(ui,BodyId::Jupiter,UiCommand::Landmarks,lx,ly));
        assert(ui_command_center(ui,BodyId::Io,UiCommand::Landmarks,lx,ly));
        SDL_Event probe=event;
        probe.button.x=lx;
        probe.button.y=ly;
        const bool before=ui.planetary_features_visible;
        assert(ui_event(ui,probe,cam,map,globe,countries,input)); // toolbar consumes
        assert(ui.planetary_features_visible==before);
    }
    click_at(event,ui,globe.body,UiCommand::PreviousBody);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.body==BodyId::Mars);
    click_at(event,ui,globe.body,UiCommand::NextBody);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.body==BodyId::Io);
    // The non-Earth third row has a landmark toggle, not Earth cities.
    click_at(event,ui,globe.body,UiCommand::Landmarks);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.planetary_features_visible);
    event.type=SDL_EVENT_KEY_DOWN;
    event.key.key=SDLK_F;
    event.key.repeat=false;
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(ui.planetary_features_visible);
    // Resetting Io is its prime meridian, never the machine timezone.
    click_at(event,ui,globe.body,UiCommand::Reset);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(std::abs(map.center_lon)<1e-6);
    assert(std::abs(map.center_lat)<1e-6);
    globe_select_body(globe,BodyId::Earth);

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

    // Main toolbar is at the bottom; hit-testing follows drawing.
    click_at(event,ui,globe.body,UiCommand::Grid);
    assert(event.button.y>ui.viewport_h-160);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(ui.grid_visible);

    // Draw the grid at the equator and at the pole with identical zoom.
    {
        Ui grid_ui{};
        grid_ui.grid_visible=true;
        const float aspect=static_cast<float>(grid_ui.viewport_w)/grid_ui.viewport_h;
        for (bool flat : {false,true}) {
            Camera grid_cam{};
            MapView grid_map{};
            grid_map.globe_to_flat=flat ? 1.f : 0.f;
            camera_center_on(grid_cam,0.,0.);
            ui_draw_graticule(grid_ui,grid_cam,grid_map,aspect);
            const double wide_step=grid_ui.grid_lat_step;
            grid_cam.distance=1.08f;
            grid_map.zoom=MapView::max_zoom;
            grid_ui.triangles.clear();
            ui_draw_graticule(grid_ui,grid_cam,grid_map,aspect);
            const double close_step=grid_ui.grid_lat_step;
            assert(close_step<wide_step);
            assert(grid_ui.grid_lon_step==close_step);
            camera_center_on(grid_cam,90.,90.);
            grid_map.center_lat=std::acos(0.);
            grid_map.roll=.8;
            grid_ui.triangles.clear();
            ui_draw_graticule(grid_ui,grid_cam,grid_map,aspect);
            assert(grid_ui.grid_lat_step==close_step);
            assert(grid_ui.grid_lon_step==close_step);
        }
    }

    // New third-row overlays are independent, and can be toggled separately.
    click_at(event,ui,globe.body,UiCommand::Capitals);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.capitals_visible && ui.cities_visible && ui.rivers_visible);
    click_at(event,ui,globe.body,UiCommand::Cities);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.cities_visible);
    click_at(event,ui,globe.body,UiCommand::Rivers);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(!ui.rivers_visible);

    // Optional Relief layer is selectable without affecting political fills.
    // The default/absent dataset must not silently select nonexistent tiles.
    click_at(event,ui,globe.body,UiCommand::Relief);
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
    click_at(event,ui,globe.body,UiCommand::Elevation);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery); // missing optional dataset
    globe.elevation.preview.id=1;
    globe.elevation.cells.resize(1);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Elevation);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery);

    // A population-density layer has an independent toggle and visibility.
    click_at(event,ui,globe.body,UiCommand::Population);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery); // no dataset
    globe.population.preview.id=1;
    globe.population.cells.resize(1);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Population && globe.texture_visible);
    assert(ui_event(ui,event,cam,map,globe,countries,input));
    assert(globe.surface==SurfaceLayer::Imagery);

    // The passive ruler consumes clicks, but the map gap above it does not.
    {
        const auto p=ui_panels(ui);
        event.button.x = p.scale[0]+p.scale[2]*.5f;
        event.button.y = p.scale[1]+p.scale[3]*.5f;
        assert(ui_event(ui,event,cam,map,globe,countries,input));
        event.button.y = p.scale[1]-4.f;
        assert(!inside(p.toolbar,event.button.x,event.button.y));
        assert(!ui_event(ui,event,cam,map,globe,countries,input));
    }

    // Clicking the compass rose aligns North up, like the NORTH UP button.
    {
        const auto p=ui_panels(ui);
        map.roll=.6;
        event.button.x = p.compass[0]+40.f;
        event.button.y = p.compass[1]+40.f;
        assert(ui_event(ui,event,cam,map,globe,countries,input));
        assert(ui.hover==static_cast<int>(UiCommand::NorthUp));
    }

    // North Lock is a separate compass button.
    click_at(event,ui,globe.body,UiCommand::NorthLock);
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
