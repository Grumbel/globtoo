#pragma once
#include "camera.hpp"
#include "map_view.hpp"
#include "shader.hpp"

// A flat map near a region has no seam on screen; skip redundant +/-360°
// copies of the high-resolution meshes when zoomed in.
bool scene_map_seam_visible(const MapView& map, float aspect, int lat_band = 0);

// Set identical globe/map morph uniforms for sphere, fills, and borders.
void scene_uniforms(const Shader& shader, const Camera& camera, float aspect,
                    const MapView& map, float wrap = 0.f, int lat_band = 0);
// Project a geographic anchor to normalized device coordinates. Returns false
// if outside the camera frustum, hidden behind globe, or behind flat map seam.
bool scene_project(const Camera& camera, float aspect, const MapView& map,
                   double longitude_deg, double latitude_deg,
                   float& screen_x, float& screen_y);
