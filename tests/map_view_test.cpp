// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
    // Reciprocal zooms: one up + one down should cancel, with roughly half
    // the prior step size. This also catches accidental zoom-limit changes.
    cam.distance=3.f;
    camera_apply_zoom(cam,1.f);
    assert(near(cam.distance,3.f/1.065f,1e-5));
    camera_apply_zoom(cam,-1.f);
    assert(near(cam.distance,3.f,1e-5));
    MapView wheel{};
    map_zoom(wheel,1.f);
    assert(near(wheel.zoom,1.075,1e-5));
    map_zoom(wheel,-1.f);
    assert(near(wheel.zoom,1.,1e-5));
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

    // Middle-button grab is a screen-space sheet translation, NOT geographic
    // reprojection. At every zoom, a 75x40 pixel drag moves the map by exactly
    // 75x40 pixels; the map frame, compass and geographic center stay put.
    for (auto projection : {MapProjection::Equirectangular, MapProjection::Mercator,
                            MapProjection::Mollweide, MapProjection::Hammer}) {
        MapView sheet{};
        sheet.selected=sheet.from=sheet.to=projection;
        sheet.flat_goal=sheet.globe_to_flat=1.f;
        sheet.center_lon=.4;
        sheet.center_lat=.6;
        sheet.roll=.7;
        for (float zoom : {1.f, 25.f, 80.f}) {
            sheet.zoom=zoom;
            sheet.pan_x=sheet.pan_y=0.;
            const auto saved_frame=map_frame(sheet);
            const auto saved_geo=map_geographic_center(sheet);
            const auto heading=map_north_heading(sheet,cam);
            map_grab_pan(sheet,75.f,-40.f,800);
            const double unit=2.*std::tan(.35)*sheet.map_distance/800.;
            assert(near(sheet.pan_x,75.*unit,1e-6));
            assert(near(sheet.pan_y,40.*unit,1e-6));
            assert(near(map_north_heading(sheet,cam),heading,1e-9));
            const auto geo=map_geographic_center(sheet);
            assert(near(geo[0],saved_geo[0],1e-9) && near(geo[1],saved_geo[1],1e-9));
            const auto new_frame=map_frame(sheet);
            for (int k=0;k<3;k++) {
                assert(near(saved_frame.east[k],new_frame.east[k],1e-9));
                assert(near(saved_frame.north[k],new_frame.north[k],1e-9));
                assert(near(saved_frame.center[k],new_frame.center[k],1e-9));
            }
            map_grab_pan(sheet,-75.f,40.f,800);
            assert(near(sheet.pan_x,0.,1e-6) && near(sheet.pan_y,0.,1e-6));
        }
        sheet.zoom=7.f;
        map_grab_pan(sheet,100.f,70.f,800);
        const auto old_x=sheet.pan_x, old_y=sheet.pan_y;
        // Projection changes retain the sheet translation.
        map_select(sheet,MapProjection::Equirectangular,cam);
        assert(near(sheet.pan_x,old_x) && near(sheet.pan_y,old_y));
        // Full reset clears it, as does returning from the globe.
        map_reset_navigation(sheet);
        assert(sheet.pan_x==0. && sheet.pan_y==0.);
    }
    // A screen-space grab must also update the physical location under the
    // screen center. Verify inverse projection for every 2D map, including
    // high zoom, roll, and a geographic reference frame away from the equator.
    for (auto projection : {MapProjection::Equirectangular, MapProjection::Mercator,
                            MapProjection::Mollweide, MapProjection::Hammer}) {
        MapView m{};
        m.selected=m.from=m.to=projection;
        m.flat_goal=m.globe_to_flat=1.f;
        m.zoom=30.f;
        m.roll=.35;
        m.center_lon=.7;
        m.center_lat=.4;
        const double local_lon=.42, local_lat=.27;
        const auto xy=map_project(projection,local_lon,local_lat);
        const double cr=std::cos(m.roll), sr=std::sin(m.roll);
        m.pan_x=-(cr*xy[0]-sr*xy[1])*m.zoom;
        m.pan_y=-(sr*xy[0]+cr*xy[1])*m.zoom;
        const auto at_center=map_sheet_center(m);
        assert(at_center);
        // Rolling a panned map must rotate the translation as well, keeping
        // the same geographic point under the viewport center.
        map_apply_roll(m,20.f);
        const auto after_roll=map_sheet_center(m);
        assert(after_roll);
        assert(near(after_roll->at(0),at_center->at(0),1e-6));
        assert(near(after_roll->at(1),at_center->at(1),1e-6));
        const auto actual=map_relative_lonlat(m,(*at_center)[0],(*at_center)[1]);
        assert(near(actual[0],local_lon,1e-6));
        assert(near(actual[1],local_lat,1e-6));
        // Folding into a 3D globe should center the location the user slid
        // beneath the screen, not the old (now off-screen) map origin.
        Camera globe_camera{};
        map_select(m,MapProjection::Globe,globe_camera);
        const Vec3 facing=camera_view_to_world(globe_camera,{0,0,1});
        const auto ll=geo::xyz_to_lonlat(facing.x,facing.y,facing.z);
        assert(near(ll[0]*geo::to_rad,(*at_center)[0],1e-5));
        assert(near(ll[1]*geo::to_rad,(*at_center)[1],1e-5));
    }
    // If a bounded oval has been pulled completely off screen, there is no
    // geographic sheet center to invent.
    MapView oval{};
    oval.flat_goal=oval.globe_to_flat=1.f;
    oval.from=oval.to=oval.selected=MapProjection::Hammer;
    oval.pan_x=100.;
    assert(!map_sheet_center(oval));

    // Longitude wraps endlessly in cylindrical maps even after dragging many
    // complete worlds, including when the sheet has been rolled.
    for (auto projection : {MapProjection::Equirectangular, MapProjection::Mercator}) {
        MapView wrapped{};
        wrapped.selected=wrapped.from=wrapped.to=projection;
        wrapped.flat_goal=wrapped.globe_to_flat=1.f;
        wrapped.zoom=10.f;
        wrapped.roll=.8;
        const auto before=map_frame(wrapped);
        for (int i=0;i<300;i++) map_grab_pan(wrapped,800.f,500.f,800);
        const double along=std::cos(wrapped.roll)*wrapped.pan_x+
                           std::sin(wrapped.roll)*wrapped.pan_y;
        assert(std::abs(along)<=geo::pi*wrapped.zoom+1e-6);
        const auto after=map_frame(wrapped);
        for (int k=0;k<3;k++) assert(near(before.center[k],after.center[k],1e-9));
    }
    MapView sphere_pan{};
    map_grab_pan(sphere_pan,100.f,100.f,800);
    assert(sphere_pan.pan_x==0. && sphere_pan.pan_y==0.);

    // Reprojection rotates the geographic globe, not a map sheet. Moving
    // across a pole must change the *positions of every country*, not create
    // a mirrored copy of a flat 2D projection.
    MapView polar{};
    polar.flat_goal = polar.globe_to_flat = 1.f;
    polar.selected = polar.from = polar.to = MapProjection::Equirectangular;
    polar.zoom = 2.f;
    polar.center_lon = .3;
    polar.center_lat = 80*geo::to_rad;
    const auto starting_center=map_geographic_center(polar);
    auto local=map_relative_lonlat(polar,starting_center[0],starting_center[1]);
    assert(near(local[0],0.) && near(local[1],0.));
    map_pan(polar,0,200,800); // drag downward through the north pole
    assert(polar.navigation_active);
    assert(polar.center_lat > 0.0 && polar.center_lat < 80*geo::to_rad);
    auto physical=map_geographic_center(polar);
    assert(near(physical[0],std::remainder(.3+geo::pi,2*geo::pi),.001));
    assert(physical[1]<geo::pi*.5 && physical[1]>0);
    local=map_relative_lonlat(polar,physical[0],physical[1]);
    assert(near(local[0],0.) && near(local[1],0.));
    // Crossing the pole must preserve the visible direction of our OLD
    // viewport center on the rotating map, not mirror its old 2D coordinate.
    const auto old_local=map_relative_lonlat(polar,starting_center[0],starting_center[1]);
    assert(old_local[1]<0.);
    polar.center_lat=-80*geo::to_rad;
    map_pan(polar,0,-200,800); // drag upward through the south pole
    assert(polar.navigation_active);
    assert(polar.center_lat > -geo::pi*.5);
    physical=map_geographic_center(polar);
    assert(physical[1]<0 && physical[1]>-geo::pi*.5);

    // Rotated tangent frames are orthogonal, centered, and agree with the
    // physical globe across +/-90, +/-180 and full-circuit crossings.
    for (auto kind : {MapProjection::Equirectangular,MapProjection::Mercator,
                      MapProjection::Mollweide,MapProjection::Hammer}) {
        for (double lat : {-220.,-100.,-90.,-80.,0.,45.,89.,90.,100.,220.}) {
            MapView m{};
            m.from=m.to=m.selected=kind;
            m.center_lon=.9;
            m.center_lat=lat*geo::to_rad;
            const auto f=map_frame(m);
            auto dot=[](const auto& a,const auto& b) {
                return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
            };
            assert(near(dot(f.center,f.center),1.,1e-10));
            assert(near(dot(f.center,f.east),0.,1e-10));
            assert(near(dot(f.center,f.north),0.,1e-10));
            assert(near(dot(f.east,f.north),0.,1e-10));
            const auto center=map_geographic_center(m);
            const auto origin=map_relative_lonlat(m,center[0],center[1]);
            assert(near(origin[0],0.,1e-10));
            assert(near(origin[1],0.,1e-10));
            const auto projected=map_project(kind,origin[0],origin[1]);
            assert(near(projected[0],0.,1e-9) && near(projected[1],0.,1e-9));
            // Any real point on Earth has one local reprojected position.
            const auto west=map_relative_lonlat(m,-120*geo::to_rad,35*geo::to_rad);
            assert(std::abs(west[0])<=geo::pi && std::abs(west[1])<=geo::pi*.5);
        }
    }
    // The moving 180-degree seam is unwrapped per primitive; vertices near
    // opposite sides must be adjacent, not a map-width-spanning triangle.
    const double base=179.7*geo::to_rad;
    const double across=-179.8*geo::to_rad;
    assert(std::abs(map_unwrap_lon(across,base)-base)<geo::to_rad);
    for (auto kind : {MapProjection::Equirectangular,MapProjection::Mercator,
                      MapProjection::Mollweide,MapProjection::Hammer})
        assert(map_world_width(kind)>=5.6);
    // Sideways panning at exactly 90 degrees must actually move the center.
    // An Euler longitude-only pan would spin in place at this singularity.
    for (double pole : {-90.,90.}) {
        MapView nav{};
        nav.flat_goal=nav.globe_to_flat=1.f;
        nav.center_lon=.25;
        nav.center_lat=pole*geo::to_rad;
        nav.zoom=4.f;
        nav.from=nav.to=nav.selected=MapProjection::Equirectangular;
        const auto old=map_frame(nav);
        map_pan(nav,60.f,0.f,800);
        const auto next=map_frame(nav);
        const double shift=std::acos(std::clamp(
            old.center[0]*next.center[0]+old.center[1]*next.center[1]+
            old.center[2]*next.center[2],-1.,1.));
        assert(shift>.01 && shift<.5);
        const auto c=map_geographic_center(nav);
        const auto local_origin=map_relative_lonlat(nav,c[0],c[1]);
        assert(near(local_origin[0],0.,1e-8) && near(local_origin[1],0.,1e-8));
        assert(near(next.east[0]*next.north[0]+next.east[1]*next.north[1]+
                    next.east[2]*next.north[2],0.,1e-8));
        // The compass, North-up button and 3D fold must agree after
        // navigating sideways out of a pole.
        map_apply_roll(nav,99.f);
        map_align_north(nav,cam);
        assert(near(map_north_heading(nav,cam),0.,1e-6));
        const auto saved=map_geographic_center(nav);
        map_select(nav,MapProjection::Globe,cam);
        const auto cam_point=camera_view_to_world(cam,{0,0,1});
        const auto facing_geo=geo::xyz_to_lonlat(cam_point.x,cam_point.y,cam_point.z);
        assert(near(std::remainder(facing_geo[0]*geo::to_rad-saved[0],2*geo::pi),0.,.001));
        assert(near(facing_geo[1]*geo::to_rad,saved[1],.001));
        assert(near(map_north_heading(nav,cam),0.,.003));
    }
    // Rotations by entire great circles are periodic. After 24 equal north
    // drags corresponding to one full revolution, the same geography must
    // return to the center with the same map orientation.
    {
        MapView orbit{};
        orbit.flat_goal=orbit.globe_to_flat=1.f;
        orbit.center_lon=.2;
        orbit.center_lat=.5;
        const auto origin=map_frame(orbit);
        const float one_circle_pixels=static_cast<float>(
            (2*geo::pi/24.)*800./(2.*std::tan(.35)*orbit.map_distance));
        for (int i=0;i<24;i++) map_pan(orbit,0.f,one_circle_pixels,800);
        const auto result=map_frame(orbit);
        for (int k=0;k<3;k++) {
            assert(near(result.center[k],origin.center[k],.0001));
            assert(near(result.north[k],origin.north[k],.0001));
            assert(near(result.east[k],origin.east[k],.0001));
        }
    }
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
    const double original_north = map_north_heading(polar,cam);
    map_select(polar,MapProjection::Globe,cam);
    assert(near(std::remainder(map_north_heading(polar,cam)-original_north,
                               2*geo::pi),0.,.02));
    facing = camera_view_to_world(cam,{0,0,1});
    const auto globe_center = geo::xyz_to_lonlat(facing.x,facing.y,facing.z);
    assert(near(globe_center[0]*geo::to_rad,
                std::remainder(.1+geo::pi,2*geo::pi),.01));
    assert(near(globe_center[1],80,.01));
    map_select(polar,MapProjection::Equirectangular,cam);
    // On a reflected polar sheet the saved roll changes by 180 degrees,
    // but geographic North must *not* jump during the fold/unfold.
    assert(near(std::remainder(map_north_heading(polar,cam)-original_north,
                               2*geo::pi),0.,.02));

    // The compass must show geographic, not screen-space or sheet-space,
    // North. All rotations must be reversible without affecting location.
    for (double latitude : {-89.,-55.,0.,42.,88.}) {
        Camera globe_cam{};
        MapView globe_map{};
        camera_center_on(globe_cam, 124.,latitude);
        globe_cam.distance=1.16f;
        assert(near(map_north_heading(globe_map,globe_cam),0.,.001));
        const Vec3 before=camera_view_to_world(globe_cam,{0,0,1});
        camera_apply_roll(globe_cam,127.f,.01f);
        assert(near(map_north_heading(globe_map,globe_cam),-1.27,.002));
        map_align_north(globe_map,globe_cam);
        assert(near(map_north_heading(globe_map,globe_cam),0.,.002));
        const Vec3 after=camera_view_to_world(globe_cam,{0,0,1});
        assert(near(before.x,after.x,.0001) &&
               near(before.y,after.y,.0001) &&
               near(before.z,after.z,.0001));
        assert(near(globe_cam.distance,1.16));
        assert(near(globe_cam.ang_vel.x,0) &&
               near(globe_cam.ang_vel.y,0) &&
               near(globe_cam.ang_vel.z,0));
    }
    for (auto kind : {MapProjection::Equirectangular, MapProjection::Mercator,
                      MapProjection::Mollweide, MapProjection::Hammer}) {
        for (double sheet_lat : {40.,100.,-100.,220.}) {
            MapView flat{};
            flat.selected=flat.from=flat.to=kind;
            flat.flat_goal=flat.globe_to_flat=1.f;
            flat.center_lat=sheet_lat*geo::to_rad;
            flat.center_lon=-.6;
            flat.zoom=20.f;
            flat.roll=.73;
            const auto center=map_geographic_center(flat);
            const double heading=(std::cos(flat.center_lat)<0 ? geo::pi : 0.)-.73;
            assert(near(map_north_heading(flat,cam),
                        std::remainder(heading,2*geo::pi),.001));
            map_align_north(flat,cam);
            assert(near(map_north_heading(flat,cam),0.,.001));
            assert(near(flat.center_lon,-.6) &&
                   near(flat.center_lat,sheet_lat*geo::to_rad) &&
                   near(flat.zoom,20.));
            auto aligned_center=map_geographic_center(flat);
            assert(near(center[0],aligned_center[0]) &&
                   near(center[1],aligned_center[1]));
            // Folding to the globe keeps geographic North at the top even
            // after crossing a pole, and unfolding does not invert it.
            Camera folded{};
            map_select(flat,MapProjection::Globe,folded);
            assert(near(map_north_heading(flat,folded),0.,.002));
            map_select(flat,kind,folded);
            assert(near(map_north_heading(flat,folded),0.,.002));
        }
    }


    // TZ database coordinate lookup, when available, supersedes coarse UTC.
    setenv("TZ", "Europe/Berlin", 1);
    const auto berlin = timezone_center();
    if (berlin[1] != 0.) {
        assert(near(berlin[0],13.3667,.2));
        assert(near(berlin[1],52.5,.2));
    }
}
