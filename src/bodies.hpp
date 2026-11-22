// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <string_view>

// Geographic coordinates in Globtoo's viewer are always longitude-positive
// east and latitude-positive north. This is an internal rendering convention,
// not a claim that an arbitrary IAU/USGS raster uses the same convention.
// External planetary maps must be normalized to this convention before use.
enum class BodyId { Earth, Moon, Mars, Io };

struct BodyInfo {
    BodyId id;
    std::string_view name;
    std::string_view default_texture_resource;
    std::string_view default_imagery_manifest;
    double mean_radius_km;
    std::array<float,3> untextured_rgb;
    bool earth_geography;
};

inline constexpr std::array<BodyInfo,4> body_catalog{{
    {BodyId::Earth,"Earth","assets/earth.jpg","imagery/manifest.txt",6371.0,  {0.25f,0.42f,0.62f},true},
    {BodyId::Moon, "Moon", "assets/moon.jpg", "imagery/moon/manifest.txt", 1737.4,  {0.46f,0.46f,0.48f},false},
    {BodyId::Mars, "Mars", "assets/mars.jpg", "imagery/mars/manifest.txt", 3389.5,  {0.63f,0.32f,0.22f},false},
    {BodyId::Io,   "Io",   "assets/io.jpg",   "imagery/io/manifest.txt",   1821.6,  {0.75f,0.63f,0.31f},false}
}};

constexpr const BodyInfo& body_info(BodyId id) {
    return body_catalog[static_cast<std::size_t>(id)];
}
constexpr BodyId next_body(BodyId id) {
    return body_catalog[(static_cast<std::size_t>(id)+1)%body_catalog.size()].id;
}
inline bool parse_body(std::string_view name, BodyId& id) {
    for (const auto& body : body_catalog) {
        if (body.name.size() != name.size()) continue;
        bool match=true;
        for (std::size_t i=0;i<name.size();i++) {
            char c=name[i];
            if (c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
            char expected=body.name[i];
            if (expected>='A' && expected<='Z') expected=static_cast<char>(expected-'A'+'a');
            if (c!=expected) { match=false; break; }
        }
        if (match) { id=body.id; return true; }
    }
    return false;
}
