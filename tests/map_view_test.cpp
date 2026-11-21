#include "map_view.hpp"
#include "geocoordinates.hpp"
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
    map_select(map,MapProjection::Globe,cam);
    for (int i=0;i<120;i++) map_update(map,.016f);
    assert(near(map.globe_to_flat,0.f,.001));
    // TZ database coordinate lookup, when available, supersedes coarse UTC.
    setenv("TZ", "Europe/Berlin", 1);
    const auto berlin = timezone_center();
    if (berlin[1] != 0.) {
        assert(near(berlin[0],13.3667,.2));
        assert(near(berlin[1],52.5,.2));
    }
}
