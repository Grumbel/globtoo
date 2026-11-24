// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "bodies.hpp"
#include <cassert>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

int main() {
    BodyId id=BodyId::Earth;
    assert(parse_body("mArS",id) && id==BodyId::Mars);
    assert(parse_body("TITAN",id) && id==BodyId::Titan);
    assert(parse_body("venus",id) && id==BodyId::Venus);
    assert(!parse_body("phobos",id));
    assert(!parse_body("",id));
    // Selection order: heliocentric, each planet followed by its moons; wraps.
    assert(next_body(BodyId::Earth)==BodyId::Moon);
    assert(next_body(BodyId::Moon)==BodyId::Mars);
    assert(next_body(BodyId::Mars)==BodyId::Jupiter);
    assert(next_body(BodyId::Jupiter)==BodyId::Io);
    assert(next_body(BodyId::Charon)==BodyId::Mercury);
    assert(previous_body(BodyId::Mercury)==BodyId::Charon);
    for (const auto& b:body_catalog) {
        assert(previous_body(next_body(b.id))==b.id);
        assert(b.mean_radius_km>100. && b.mean_radius_km<100000.);
        assert(!b.slug.empty() && !b.name.empty());
        if (b.kind==BodyKind::Satellite) {
            const auto& parent=body_info(b.parent);
            assert(parent.kind!=BodyKind::Satellite);
            assert(parent.mean_radius_km>b.mean_radius_km);
            // Moons follow their parent (directly or after siblings).
            assert(body_index(parent.id)<body_index(b.id));
        }
        assert(b.earth_geography==(b.id==BodyId::Earth));
        if (b.id!=BodyId::Earth) {
            assert(b.default_texture_resource=="assets/"+std::string(b.slug)+".jpg");
            assert(b.default_imagery_manifest=="imagery/"+std::string(b.slug)+"/manifest.txt");
        }
    }
    assert(!body_info(BodyId::Jupiter).solid_surface);
    assert(body_info(BodyId::Titan).solid_surface);
    assert(body_info(BodyId::Charon).parent==BodyId::Pluto);
    assert(body_info(BodyId::Pluto).kind==BodyKind::DwarfPlanet);
    assert(body_info(BodyId::Earth).default_texture_resource=="assets/earth.jpg");
    assert(body_info(BodyId::Earth).default_imagery_manifest=="imagery/manifest.txt");

    // data/bodies.tsv drives the Python tools and CMake packaging loop.
#ifdef GLOBTOO_BODIES_TSV
    std::ifstream table(GLOBTOO_BODIES_TSV);
    assert(table.good());
    std::string line;
    std::size_t count=0;
    while (std::getline(table,line)) {
        if (line.empty() || line.front()=='#') continue;
        std::istringstream record(line);
        std::array<std::string,6> field;
        for (auto& value:field) assert(static_cast<bool>(std::getline(record,value,'\t')));
        std::string extra;
        assert(!std::getline(record,extra,'\t'));
        assert(count<body_catalog.size());
        const auto& body=body_catalog[count++];
        assert(field[0]==body.slug);
        assert(field[1]==body.name);
        assert(field[2]==(body.kind==BodyKind::Planet ? "planet" :
                          body.kind==BodyKind::DwarfPlanet ? "dwarf-planet" : "moon"));
        assert(field[3]==(body.parent==body.id ? std::string("-") :
                          std::string(body_info(body.parent).slug)));
        assert(std::abs(std::stod(field[4])-body.mean_radius_km)<1e-6);
        assert(field[5]==(body.solid_surface ? "solid" : "clouds"));
    }
    assert(count==body_catalog.size());
#endif
}
