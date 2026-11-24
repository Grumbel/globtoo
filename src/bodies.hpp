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
//
// The catalog order is the selection order (M / Shift+M, NEXT / PREV):
// heliocentric, each planet followed by its rendered moons. The same bodies,
// in the same order, are listed in data/bodies.tsv for the Python tools and
// CMake; bodies_test keeps the two in agreement.
//
// Every body is drawn as a sphere of its IAU mean radius. Gas giants are
// visibly oblate and have no solid surface (their maps are cloud tops);
// small irregular moons such as Phobos and Deimos are intentionally absent
// until a body-shape model exists.
enum class BodyId {
    Mercury, Venus, Earth, Moon, Mars,
    Jupiter, Io, Europa, Ganymede, Callisto,
    Saturn, Mimas, Enceladus, Tethys, Dione, Rhea, Titan, Iapetus,
    Uranus, Miranda, Ariel, Umbriel, Titania, Oberon,
    Neptune, Triton,
    Pluto, Charon
};

enum class BodyKind { Planet, DwarfPlanet, Satellite };

struct BodyInfo {
    BodyId id;
    std::string_view name;
    std::string_view slug;      // lower-case CLI/file name ("io", "titan")
    BodyKind kind;
    BodyId parent;              // == id for bodies orbiting the Sun
    std::string_view default_texture_resource;
    std::string_view default_imagery_manifest;
    double mean_radius_km;      // IAU WGCCRE 2015 / NASA fact sheets
    std::array<float,3> untextured_rgb;
    bool solid_surface;         // false: gas/ice giant cloud tops
    bool earth_geography;
};

namespace body_detail {
using enum BodyId;
using enum BodyKind;
#define GLOBTOO_BODY(id, name, slug, kind, parent, radius, r, g, b, solid) \
    {id, name, slug, kind, parent, "assets/" slug ".jpg", \
     "imagery/" slug "/manifest.txt", radius, {r, g, b}, solid, false}
inline constexpr std::array<BodyInfo,28> catalog{{
    GLOBTOO_BODY(Mercury,  "Mercury",  "mercury",  Planet, Mercury,  2439.4, .55f,.53f,.50f, true),
    GLOBTOO_BODY(Venus,    "Venus",    "venus",    Planet, Venus,    6051.8, .78f,.62f,.38f, true),
    // Earth keeps its historical, unprefixed imagery tile directory.
    {Earth, "Earth", "earth", Planet, Earth, "assets/earth.jpg", "imagery/manifest.txt",
     6371.0, {.25f,.42f,.62f}, true, true},
    GLOBTOO_BODY(Moon,     "Moon",     "moon",     Satellite,   Earth,    1737.4, .46f,.46f,.48f, true),
    GLOBTOO_BODY(Mars,     "Mars",     "mars",     Planet, Mars,     3389.5, .63f,.32f,.22f, true),
    GLOBTOO_BODY(Jupiter,  "Jupiter",  "jupiter",  Planet, Jupiter, 69911.0, .76f,.66f,.54f, false),
    GLOBTOO_BODY(Io,       "Io",       "io",       Satellite,   Jupiter,  1821.6, .75f,.63f,.31f, true),
    GLOBTOO_BODY(Europa,   "Europa",   "europa",   Satellite,   Jupiter,  1560.8, .72f,.66f,.56f, true),
    GLOBTOO_BODY(Ganymede, "Ganymede", "ganymede", Satellite,   Jupiter,  2631.2, .52f,.48f,.44f, true),
    GLOBTOO_BODY(Callisto, "Callisto", "callisto", Satellite,   Jupiter,  2410.3, .38f,.34f,.30f, true),
    GLOBTOO_BODY(Saturn,   "Saturn",   "saturn",   Planet, Saturn,  58232.0, .82f,.73f,.55f, false),
    GLOBTOO_BODY(Mimas,    "Mimas",    "mimas",    Satellite,   Saturn,    198.2, .66f,.66f,.66f, true),
    GLOBTOO_BODY(Enceladus,"Enceladus","enceladus",Satellite,   Saturn,    252.1, .86f,.88f,.90f, true),
    GLOBTOO_BODY(Tethys,   "Tethys",   "tethys",   Satellite,   Saturn,    531.0, .80f,.80f,.80f, true),
    GLOBTOO_BODY(Dione,    "Dione",    "dione",    Satellite,   Saturn,    561.4, .72f,.72f,.72f, true),
    GLOBTOO_BODY(Rhea,     "Rhea",     "rhea",     Satellite,   Saturn,    763.5, .70f,.70f,.70f, true),
    GLOBTOO_BODY(Titan,    "Titan",    "titan",    Satellite,   Saturn,   2574.7, .78f,.58f,.30f, true),
    GLOBTOO_BODY(Iapetus,  "Iapetus",  "iapetus",  Satellite,   Saturn,    734.3, .52f,.46f,.40f, true),
    GLOBTOO_BODY(Uranus,   "Uranus",   "uranus",   Planet, Uranus,  25362.0, .62f,.82f,.86f, false),
    GLOBTOO_BODY(Miranda,  "Miranda",  "miranda",  Satellite,   Uranus,    235.8, .58f,.58f,.58f, true),
    GLOBTOO_BODY(Ariel,    "Ariel",    "ariel",    Satellite,   Uranus,    578.9, .62f,.62f,.62f, true),
    GLOBTOO_BODY(Umbriel,  "Umbriel",  "umbriel",  Satellite,   Uranus,    584.7, .40f,.40f,.40f, true),
    GLOBTOO_BODY(Titania,  "Titania",  "titania",  Satellite,   Uranus,    788.9, .56f,.54f,.52f, true),
    GLOBTOO_BODY(Oberon,   "Oberon",   "oberon",   Satellite,   Uranus,    761.4, .50f,.47f,.44f, true),
    GLOBTOO_BODY(Neptune,  "Neptune",  "neptune",  Planet, Neptune, 24622.0, .30f,.45f,.85f, false),
    GLOBTOO_BODY(Triton,   "Triton",   "triton",   Satellite,   Neptune,  1352.6, .76f,.70f,.66f, true),
    GLOBTOO_BODY(Pluto,    "Pluto",    "pluto",    DwarfPlanet, Pluto, 1188.3, .76f,.64f,.52f, true),
    GLOBTOO_BODY(Charon,   "Charon",   "charon",   Satellite,   Pluto,     606.0, .56f,.54f,.52f, true),
}};
#undef GLOBTOO_BODY
} // namespace body_detail

inline constexpr const auto& body_catalog = body_detail::catalog;

constexpr std::size_t body_index(BodyId id) { return static_cast<std::size_t>(id); }
constexpr const BodyInfo& body_info(BodyId id) {
    return body_catalog[body_index(id)];
}
constexpr BodyId next_body(BodyId id) {
    return body_catalog[(body_index(id)+1)%body_catalog.size()].id;
}
constexpr BodyId previous_body(BodyId id) {
    return body_catalog[(body_index(id)+body_catalog.size()-1)%body_catalog.size()].id;
}
constexpr std::string_view body_kind_name(BodyKind kind) {
    return kind==BodyKind::Planet ? "planet" :
           kind==BodyKind::DwarfPlanet ? "dwarf planet" : "moon";
}

// Case-insensitive match against the display name ("Io") or slug ("io").
constexpr bool parse_body(std::string_view name, BodyId& id) {
    const auto lower=[](char c) {
        return c>='A' && c<='Z' ? static_cast<char>(c-'A'+'a') : c;
    };
    for (const auto& body : body_catalog) {
        if (body.slug.size() != name.size()) continue;
        bool match=true;
        for (std::size_t i=0;i<name.size();i++)
            if (lower(name[i])!=body.slug[i]) { match=false; break; }
        if (match) { id=body.id; return true; }
    }
    return false;
}

static_assert([] {
    for (std::size_t i=0;i<body_catalog.size();i++) {
        const auto& body=body_catalog[i];
        if (body_index(body.id)!=i) return false;
        if (body_index(body.parent)>=body_catalog.size()) return false;
        if ((body.kind==BodyKind::Satellite)==(body.parent==body.id)) return false;
        for (char c : body.slug)
            if (c<'a' || c>'z') return false;
    }
    return true;
}(), "body catalog order must match BodyId; moons need a parent; ASCII slugs");
