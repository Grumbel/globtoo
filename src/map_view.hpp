#pragma once

#include "camera.hpp"
#include <array>

// Geographic projections. 0 is the three-dimensional globe, 1..4 are flat.
enum class MapProjection : int { Globe = 0, Equirectangular = 1, Mercator = 2,
                                 Mollweide = 3, Hammer = 4 };

// Local tangent frame of the rotated Earth. The center faces the viewer;
// east is map-right and north is map-up before screen roll is applied.
struct MapFrame {
    std::array<double, 3> east, north, center;
};

struct MapView {
    MapProjection selected = MapProjection::Globe;
    MapProjection from = MapProjection::Equirectangular;
    MapProjection to = MapProjection::Equirectangular;
    float globe_to_flat = 0.f; // eased actual blend: 0 sphere, 1 map
    float flat_goal = 0.f;
    float projection_blend = 1.f;
    float map_distance = 5.8f;
    float zoom = 1.f;
    static constexpr float max_zoom = 80.f;
    double center_lon = 0.0; // radians, projection's central meridian
    double center_lat = 0.0; // radians; initial orientation / current geographic latitude
    double roll = 0.0;       // radians, counterclockwise on screen
    // An active frame captures the full 3D orientation after drag. Euler
    // lon/lat alone lose a degree of freedom and fail to pan at the poles.
    MapFrame navigation_frame{};
    bool navigation_active = false;
    double navigation_lon = 0.0, navigation_lat = 0.0;
};

// The CPU and GLSL formulas are kept in sync and tested at their landmarks.
std::array<double, 2> map_project(MapProjection projection, double relative_lon, double lat);
void map_select(MapView& map, MapProjection projection, Camera& cam);
void map_update(MapView& map, float dt);
void map_pan(MapView& map, float dx_pixels, float dy_pixels, int height);
void map_zoom(MapView& map, float wheel);
// Reprojection is defined by a *rotated globe* centered at the viewing point.
// These form a right-handed local tangent frame; unlike reflected map copies,
// they remain continuous as the view rotates across either geographic pole.
MapFrame map_frame(const MapView& map);
void map_reset_navigation(MapView& map);
std::array<double, 2> map_relative_lonlat(const MapView& map,
                                          double lon, double lat);
// Unwrap one projected triangle/line segment about its own center to avoid
// interpolating across a moving +/-180 degree map seam.
double map_unwrap_lon(double longitude, double reference);
// Width of a complete projected world for horizontal tiling.
double map_world_width(MapProjection projection);
std::array<double, 2> map_geographic_center(const MapView& map);
void map_apply_roll(MapView& map, float dx_pixels, float sensitivity = .005f);
// Clockwise screen bearing of geographic north at the viewport center, in
// radians (zero means north points up); follows true geographic north
// through arbitrary rotations of the map frame.
double map_north_heading(const MapView& map, const Camera& cam);
// Straighten the current view without changing its geographic center or zoom.
void map_align_north(MapView& map, Camera& cam);

void camera_center_on(Camera& cam, double longitude_degrees, double latitude_degrees);
// Approximate a location from the actual system timezone entry (zone.tab);
// fallback: UTC offset determines longitude, latitude remains equatorial.
std::array<double, 2> timezone_center(); // longitude, latitude, degrees
