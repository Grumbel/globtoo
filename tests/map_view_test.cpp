#include "map_view.hpp"
#include "geocoordinates.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <string>

static bool near(double a, double b, double eps=1e-3) { return std::abs(a-b)<eps; }
int main() {
    assert(near(map_project(MapProjection::Equirectangular, 0.,0.)[0],0.));
    assert(near(map_project(MapProjection::Equirectangular, 1., .5)[0],1.));
    for (auto kind : {MapProjection::Equirectangular, MapProjection::Mercator,
                      MapProjection::Mollweide, MapProjection::Hammer}) {
        const auto origin = map_project(kind,0,0);
        assert(near(origin[0],0) && near(origin[1],0));
        const auto east = map_project(kind,.4,.3);
        const auto west = map_project(kind,-.4,.3);
        assert(east[0]>0 && west[0]<0 && near(east[0],-west[0]));
        const auto north = map_project(kind,0,.4);
        assert(north[1]>0);
        const auto pole = map_project(kind,0, geo::pi*.5);
        assert(std::isfinite(pole[0]) && std::isfinite(pole[1]));
    }
    Camera cam{};
    camera_center_on(cam, 13.4,52.5);
    auto facing = camera_view_to_world(cam,{0,0,1});
    const auto coords = geo::xyz_to_lonlat(facing.x,facing.y,facing.z);
    assert(near(coords[0],13.4, .01) && near(coords[1],52.5,.01));
    MapView map{};
    map_select(map,MapProjection::Mercator,cam);
    assert(map.flat_goal==1.f && map.selected==MapProjection::Mercator);
    for (int i=0;i<120;i++) map_update(map,.016f);
    assert(near(map.globe_to_flat,1.f,.001));
    map_select(map,MapProjection::Hammer,cam);
    assert(map.projection_blend==0);
    for (int i=0;i<120;i++) map_update(map,.016f);
    assert(near(map.projection_blend,1.f,.001));
    map_pan(map,100.f,50.f,800);
    assert(map.center_lon < coords[0]*geo::to_rad);
    assert(map.center_lat > coords[1]*geo::to_rad);
    map_zoom(map,1);
    assert(map.zoom > 1);
    for (int i=0;i<200;i++) map_zoom(map,1);
    assert(near(map.zoom,MapView::max_zoom));
    for (int i=0;i<400;i++) map_zoom(map,-1);
    assert(near(map.zoom,.6f));
    map_select(map,MapProjection::Globe,cam);
    for (int i=0;i<120;i++) map_update(map,.016f);
    assert(near(map.globe_to_flat,0.f,.001));
    // Switching a tightly zoomed globe to a map must not reset to a
    // whole-world view. Both on-screen buttons and wheel call these paths.
    cam.distance=cam.min_dist;
    map_select(map,MapProjection::Equirectangular,cam);
    assert(map.zoom >= 12.f);
    assert(map.zoom <= MapView::max_zoom);
    map_select(map,MapProjection::Globe,cam);

    // A flat map is a continuous polar sheet rather than a latitude-clamped
    // rectangle. North of 90 degrees the geographic latitude is reflected
    // and longitude is shifted by half a turn.
    for (auto kind : {MapProjection::Equirectangular, MapProjection::Mercator,
                      MapProjection::Mollweide, MapProjection::Hammer}) {
        const double h = map_project(kind,0,geo::pi*.5)[1];
        assert(near(map_sheet_y(kind,geo::pi*.5), h));
        assert(near(map_sheet_y(kind,geo::pi*.5-.001),
                    map_sheet_y(kind,geo::pi*.5+.001), .03));
        assert(near(map_sheet_y(kind,geo::pi*1.5), 3*h));
        assert(near(map_sheet_y(kind,-geo::pi*.5),-h));
        assert(near(map_sheet_y(kind,-geo::pi*1.5),-3*h));
    }
    MapView polar{};
    polar.flat_goal = polar.globe_to_flat = 1.f;
    polar.selected = polar.from = polar.to = MapProjection::Equirectangular;
    polar.zoom = 2.f;
    polar.center_lon = .3;
    polar.center_lat = 80*geo::to_rad;
    map_pan(polar, 0, 200, 800); // drag downward through the North Pole
    assert(map_lat_band(polar.center_lat)==1);
    auto physical = map_geographic_center(polar);
    assert(near(physical[0], std::remainder(.3+geo::pi,2*geo::pi),.001));
    assert(physical[1]<geo::pi*.5 && physical[1]>0);
    const auto bands_north = map_visible_bands(polar,1.6f);
    assert(std::find(bands_north.begin(),bands_north.end(),1)!=bands_north.end());
    polar.center_lat = -80*geo::to_rad;
    map_pan(polar,0,-200,800); // drag upward through the South Pole
    assert(map_lat_band(polar.center_lat)==-1);
    physical = map_geographic_center(polar);
    assert(physical[1]<0 && physical[1]>-geo::pi*.5);
    // Shift+drag rolls the map, not the otherwise-hidden globe camera.
    polar.center_lon = .2;
    polar.center_lat = 0.;
    map_apply_roll(polar,100.f);
    assert(near(polar.roll,.5));
    const double old_lon = polar.center_lon;
    map_pan(polar,0.f,80.f,800);
    assert(polar.center_lon!=old_lon); // vertical drag also moves east/west when rolled
    // Switching back to the sphere centers on the actual reflected latitude,
    // and preserves the projected rotation as a globe roll.
    polar.center_lat = 100*geo::to_rad;
    polar.center_lon = .1;
    map_select(polar,MapProjection::Globe,cam);
    facing = camera_view_to_world(cam,{0,0,1});
    const auto globe_center = geo::xyz_to_lonlat(facing.x,facing.y,facing.z);
    assert(near(globe_center[0]*geo::to_rad,
                std::remainder(.1+geo::pi,2*geo::pi),.01));
    assert(near(globe_center[1],80,.01));
    map_select(polar,MapProjection::Equirectangular,cam);
    assert(near(polar.roll,.5,.02));

    // TZ database coordinate lookup, when available, supersedes coarse UTC.
    setenv("TZ", "Europe/Berlin", 1);
    const auto berlin = timezone_center();
    if (berlin[1] != 0.) {
        assert(near(berlin[0],13.3667,.2));
        assert(near(berlin[1],52.5,.2));
    }
}
