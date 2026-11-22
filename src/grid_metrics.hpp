#pragma once

#include <algorithm>
#include <cmath>

// Map graticules are angular, not a uniform kilometer grid. The scale bar
// uses a local geodesic derivative at the view center to give real distances.
namespace grid {
constexpr double earth_radius_km = 6371.0088; // mean spherical Earth radius

// Select an ordinary cartographic latitude/longitude interval (degrees).
// More pixels per degree means a finer grid at higher zoom levels.
inline double angular_step(double pixels_per_degree, double desired_pixels = 120.) {
    constexpr double steps[] = {0.025, 0.05, 0.1, 0.2, 0.5, 1., 2., 5., 10., 15., 30., 45., 90.};
    if (!std::isfinite(pixels_per_degree) || pixels_per_degree <= 0.) return 90.;
    for (double step : steps)
        if (step * pixels_per_degree >= desired_pixels) return step;
    return 90.;
}

// Select a round distance <= the desired bar size (1, 2, 5 x 10^n).
inline double round_distance_km(double desired_km) {
    if (!(desired_km > 0.) || !std::isfinite(desired_km)) return 0.;
    const double decade = std::pow(10., std::floor(std::log10(desired_km)));
    if (desired_km >= 5.*decade) return 5.*decade;
    if (desired_km >= 2.*decade) return 2.*decade;
    return decade;
}

// Local great-circle distance of an angular displacement, rather than
// projected degrees: longitude spacing shrinks towards geographic poles.
inline double great_circle_km(double angle_radians) {
    return earth_radius_km * std::abs(angle_radians);
}
}
