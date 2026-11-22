// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "bodies.hpp"
#include <cassert>
int main() {
    BodyId id=BodyId::Earth;
    assert(parse_body("mArS",id) && id==BodyId::Mars);
    assert(!parse_body("venus",id));
    assert(next_body(BodyId::Io)==BodyId::Earth);
    for (auto b:body_catalog) assert(b.mean_radius_km>1000.);
    assert(body_info(BodyId::Earth).earth_geography);
    assert(!body_info(BodyId::Moon).earth_geography);
}
