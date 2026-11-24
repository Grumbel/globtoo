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
// Each row keeps the coordinate system the Gazetteer publishes for its body:
// Moon:              planetographic latitude, east-positive (-180..180).
// Mars:              planetocentric latitude, east-positive (0..360).
// Mercury, Io and the Galilean/Saturnian moons: planetographic latitude,
//                    west-positive (0..360).
// Venus, Uranian moons, Pluto, Charon: planetocentric, east-positive.
// Triton:            planetographic latitude, east-positive (0..360).
// Every body with planetographic entries here has a spherical IAU reference
// surface, where planetographic == planetocentric latitude by definition;
// Mars (the only oblate one with landmarks) is listed planetocentric. Real
// mosaic control networks and elevation-dependent geolocation can still
// differ from these centers. Gas giants have no named surface features.
// https://planetarynames.wr.usgs.gov/Page/Website
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

inline constexpr std::array<PlanetaryFeature,64> planetary_features{{
    {BodyId::Mercury, "Caloris Planitia", "plain", 30.50, 189.80,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 979},
    {BodyId::Mercury, "Rachmaninoff", "crater", 27.60, 302.40,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14653},
    {BodyId::Mercury, "Kuiper", "crater", -11.30, 31.10,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 3140},
    {BodyId::Venus, "Maxwell Montes", "mountains", 65.20, 3.30,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 3766},
    {BodyId::Venus, "Aphrodite Terra", "highland", -5.80, 104.80,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 317},
    {BodyId::Venus, "Mead", "crater", 12.50, 57.20,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 3787},
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
    {BodyId::Europa, "Pwyll", "crater", -25.20, 271.40,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 4878},
    {BodyId::Europa, "Conamara Chaos", "chaos", 9.70, 272.70,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 1282},
    {BodyId::Europa, "Tyre", "ringed basin", 33.60, 146.60,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 6170},
    {BodyId::Ganymede, "Galileo Regio", "region", 45.00, 127.00,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 2076},
    {BodyId::Ganymede, "Osiris", "crater", -38.00, 166.31,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 4509},
    {BodyId::Ganymede, "Tros", "crater", 11.14, 27.26,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 6091},
    {BodyId::Callisto, "Valhalla", "ringed basin", 14.70, 56.00,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 6284},
    {BodyId::Callisto, "Asgard", "ringed basin", 32.20, 139.90,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 420},
    {BodyId::Callisto, "Bran", "crater", -24.20, 205.60,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 862},
    {BodyId::Mimas, "Herschel", "crater", -1.38, 111.76,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 2478},
    {BodyId::Mimas, "Arthur", "crater", -35.40, 196.04,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 403},
    {BodyId::Enceladus, "Baghdad Sulcus", "grooves", -86.91, 230.54,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14248},
    {BodyId::Enceladus, "Ali Baba", "crater", 56.84, 17.51,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 184},
    {BodyId::Enceladus, "Samarkand Sulci", "grooves", 30.00, 327.50,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 5291},
    {BodyId::Tethys, "Odysseus", "crater", 32.82, 128.89,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 4406},
    {BodyId::Tethys, "Ithaca Chasma", "canyon", -14.00, 6.10,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 2751},
    {BodyId::Tethys, "Penelope", "crater", -10.83, 249.22,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 4645},
    {BodyId::Dione, "Evander", "crater", -57.00, 145.00,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14353},
    {BodyId::Dione, "Padua Chasmata", "canyon", 17.70, 247.17,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14384},
    {BodyId::Dione, "Aeneas", "crater", 25.89, 46.27,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 64},
    {BodyId::Rhea, "Tirawa", "crater", 34.20, 151.70,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 6026},
    {BodyId::Rhea, "Inktomi", "crater", -14.10, 112.10,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14671},
    {BodyId::Rhea, "Mamaldi", "crater", 14.00, 184.00,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14751},
    {BodyId::Titan, "Xanadu", "albedo feature", -15.00, 100.00,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 6958},
    {BodyId::Titan, "Kraken Mare", "sea", 68.00, 310.00,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14399},
    {BodyId::Titan, "Menrva", "crater", 20.10, 87.20,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 7025},
    {BodyId::Iapetus, "Cassini Regio", "region", -28.10, 92.60,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 1047},
    {BodyId::Iapetus, "Engelier", "crater", -40.50, 264.70,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14463},
    {BodyId::Iapetus, "Turgis", "crater", 16.90, 28.40,
     FeatureLongitude::WestPositive, FeatureLatitude::Planetographic, 14488},
    {BodyId::Miranda, "Inverness Corona", "corona", -66.90, 325.70,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 2704},
    {BodyId::Miranda, "Verona Rupes", "scarp", -18.30, 347.80,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 6359},
    {BodyId::Miranda, "Arden Corona", "corona", -29.10, 73.70,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 353},
    {BodyId::Ariel, "Yangoor", "crater", -68.70, 279.70,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 6638},
    {BodyId::Ariel, "Kachina Chasmata", "canyon", -33.70, 246.00,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 2879},
    {BodyId::Umbriel, "Wunda", "crater", -7.90, 273.60,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 6587},
    {BodyId::Umbriel, "Vuver", "crater", -4.70, 311.60,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 6460},
    {BodyId::Titania, "Messina Chasmata", "canyon", -33.30, 335.00,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 3870},
    {BodyId::Titania, "Gertrude", "crater", -15.80, 287.10,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 2150},
    {BodyId::Oberon, "Hamlet", "crater", -46.10, 44.40,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 2340},
    {BodyId::Oberon, "Mommur Chasma", "canyon", -16.30, 323.50,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 3958},
    {BodyId::Triton, "Leviathan Patera", "patera", 17.00, 28.50,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetographic, 3368},
    {BodyId::Triton, "Tuonela Planitia", "plain", 34.00, 14.50,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetographic, 6134},
    {BodyId::Triton, "Mazomba", "crater", -18.50, 63.50,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetographic, 3771},
    {BodyId::Pluto, "Sputnik Planitia", "plain", 19.51, 178.69,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 15669},
    {BodyId::Pluto, "Tombaugh Regio", "region", 7.62, 183.22,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 15668},
    {BodyId::Pluto, "Burney", "crater", 45.68, 133.79,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 15680},
    {BodyId::Charon, "Dorothy", "crater", 58.53, 40.58,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 15738},
    {BodyId::Charon, "Argo Chasma", "canyon", 27.80, 80.40,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 15734},
    {BodyId::Charon, "Kubrick Mons", "mountains", 3.60, 30.80,
     FeatureLongitude::EastPositive, FeatureLatitude::Planetocentric, 15736},
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

// Gas giants (cloud tops) have no named surface features.
constexpr bool body_has_planetary_features(BodyId body) {
    for (const auto& feature : planetary_features)
        if (feature.body==body) return true;
    return false;
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
