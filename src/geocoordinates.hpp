// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cmath>

// Shared geographic/world frame (right-handed, Y up):
//   0 E / 0 N -> +X, 90 E -> -Z, 90 W -> +Z, north -> +Y.
// East must move to screen RIGHT for a camera outside the globe with north up.
// Keep this conversion shared by the sphere, GeoJSON overlay and stars.
namespace geo {
constexpr double pi = 3.14159265358979323846264338327950288;
constexpr double to_rad = pi / 180.0;
constexpr double to_deg = 180.0 / pi;

inline std::array<double, 3> lonlat_to_xyz(double lon_degrees,
                                            double lat_degrees,
                                            double radius = 1.0) {
    const double lon = lon_degrees * to_rad;
    const double lat = lat_degrees * to_rad;
    const double c = radius * std::cos(lat);
    return {c * std::cos(lon), radius * std::sin(lat), -c * std::sin(lon)};
}

// Equirectangular texture: (u,v)=(0,0) is northwest, (1,1) southeast;
// seam is on the dateline, u=0.5 is Greenwich, v=0 is the north pole.
inline std::array<double, 3> uv_to_xyz(double u, double v,
                                       double radius = 1.0) {
    return lonlat_to_xyz((u - 0.5) * 360.0, 90.0 - v * 180.0, radius);
}

inline std::array<double, 2> xyz_to_lonlat(double x, double y, double z) {
    const double r = std::sqrt(x * x + y * y + z * z);
    if (r == 0.0) return {0.0, 0.0};
    return {std::atan2(-z, x) * to_deg, std::asin(y / r) * to_deg};
}
} // namespace geo
