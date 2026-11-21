#pragma once

#include "camera.hpp"
#include <array>
#include <vector>

// Geographic projections. 0 is the three-dimensional globe, 1..4 are flat.
enum class MapProjection : int { Globe = 0, Equirectangular = 1, Mercator = 2,
                                 Mollweide = 3, Hammer = 4 };

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
    double center_lat = 0.0; // unwrapped radians; can cross either pole
    double roll = 0.0;       // radians, counterclockwise on screen
};

// The CPU and GLSL formulas are kept in sync and tested at their landmarks.
std::array<double, 2> map_project(MapProjection projection, double relative_lon, double lat);
void map_select(MapView& map, MapProjection projection, Camera& cam);
void map_update(MapView& map, float dt);
void map_pan(MapView& map, float dx_pixels, float dy_pixels, int height);
void map_zoom(MapView& map, float wheel);
// In a flat projection, latitude continues past each pole instead of
// stopping at +/-85 degrees. Successive map bands are reflected about
// the poles with a half-turn in longitude.
int map_lat_band(double unwrapped_lat);
double map_sheet_y(MapProjection projection, double unwrapped_lat);
std::array<double, 2> map_geographic_center(const MapView& map); // lon, lat in radians
std::vector<int> map_visible_bands(const MapView& map, float aspect);
void map_apply_roll(MapView& map, float dx_pixels, float sensitivity = .005f);
void camera_center_on(Camera& cam, double longitude_degrees, double latitude_degrees);
// Approximate a location from the actual system timezone entry (zone.tab);
// fallback: UTC offset determines longitude, latitude remains equatorial.
std::array<double, 2> timezone_center(); // longitude, latitude, degrees
