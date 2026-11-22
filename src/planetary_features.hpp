// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "bodies.hpp"
#include <array>
#include <cstddef>
#include <string_view>

// Selected IAU-approved names from the USGS Gazetteer of Planetary
// Nomenclature. The values below are the coordinates of the *centers* of
// named features, not their geometric outlines or observation locations.
// Preserve the longitude convention of the source entry and convert only
// when projecting into Globtoo's east-positive [-180, 180] spherical frame.
//
// Moon: planetographic latitude, east-positive longitude (-180..180).
// Mars: planetocentric latitude, east-positive longitude (0..360).
// Io:   planetographic latitude, west-positive longitude (0..360).
// Io and Moon planetographic latitudes are displayed without spheroid-to-
// sphere reprojection. This is an approximate location, not a precise datum.
enum class FeatureLongitude { EastPositive, WestPositive };
enum class FeatureLatitude { Planetocentric, Planetographic };

struct PlanetaryFeature {
    BodyId body;
    std::string_view name;
    std::string_view kind;
    double source_latitude;
    double source_longitude;
    FeatureLongitude longitude_direction;
    FeatureLatitude latitude_type;
    int gazetteer_id; // https://planetarynames.wr.usgs.gov/Feature/<id>
};

inline constexpr std::array<PlanetaryFeature,9> planetary_features{{
    {BodyId::Moon, "Tycho", "crater", -43.30, -11.22,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetographic, 6163},
    {BodyId::Moon, "Copernicus", "crater", 9.62, -20.08,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetographic, 1296},
    {BodyId::Moon, "Mare Tranquillitatis", "mare", 8.35, 30.83,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetographic, 3691},
    {BodyId::Mars, "Olympus Mons", "mountain", 18.40, 226.00,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 4453},
    {BodyId::Mars, "Gale", "crater", -5.44, 137.70,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 2071},
    {BodyId::Mars, "Hellas Planitia", "plain", -42.66, 70.00,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 2432},
    {BodyId::Io, "Loki Patera", "patera", 12.97, 308.80,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 3459},
    {BodyId::Io, "Pele", "eruptive center", -18.71, 255.28,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 4638},
    {BodyId::Io, "Tvashtar Paterae", "paterae", 62.76, 123.53,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14131},
}};

// Input is an absolute longitude measured from the source's prime meridian.
// Return canonical [-180,180), including 180 -> -180 at the map seam.
constexpr double feature_east_longitude(const PlanetaryFeature& feature) {
    double lon=feature.longitude_direction==FeatureLongitude::WestPositive ?
        -feature.source_longitude : feature.source_longitude;
    while (lon < -180.) lon += 360.;
    while (lon >= 180.) lon -= 360.;
    return lon;
}

inline const PlanetaryFeature* find_planetary_feature(std::string_view name) {
    // ASCII names in the curated table; case-insensitive CLI matching.
    const auto lower=[](char c) {
        return c>='A' && c<='Z' ? static_cast<char>(c-'A'+'a') : c;
    };
    for (const auto& feature : planetary_features) {
        if (feature.name.size()!=name.size()) continue;
        bool same=true;
        for (std::size_t i=0;i<name.size();++i)
            if (lower(feature.name[i])!=lower(name[i])) { same=false; break; }
        if (same) return &feature;
    }
    return nullptr;
}
