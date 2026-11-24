// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ring_profile.hpp"
#include <cassert>
#include <cstdio>

int main() {
    assert(!ring_profile(BodyId::Earth).valid());
    assert(!ring_profile(BodyId::Jupiter).valid());
    const RingProfile saturn = ring_profile(BodyId::Saturn);
    assert(saturn.valid());
    assert(saturn.width == 2048 && saturn.rgba.size() == 2048u * 4);
    // Covers D ring to F ring, in front of the 58232 km mean-radius globe.
    assert(saturn.inner_km > 58232 && saturn.inner_km < 74658);
    assert(saturn.outer_km > 140180 && saturn.outer_km < 145000);
    const auto opacity = [&](double km) { return ring_profile_opacity(saturn, km); };
    assert(opacity(saturn.inner_km - 1) == 0 && opacity(saturn.outer_km + 1) == 0);
    // B ring is nearly opaque and the brightest; C ring translucent.
    assert(opacity(105000) > .8);
    assert(opacity(83000) > .05 && opacity(83000) < .35);
    assert(opacity(105000) > opacity(128000) && opacity(128000) > opacity(83000));
    // Cassini Division and the Encke gap are (nearly) empty.
    assert(opacity(120000) < .2);
    assert(opacity(133589) < .05);
    assert(opacity(117680) < .05);
    // The F ring stands apart from the dark space beyond the A ring.
    assert(opacity(140180) > 2 * opacity(138500));
    for (int i = 0; i < saturn.width; i++) {
        const unsigned char* t = &saturn.rgba[i * 4u];
        // Ring particles are warm grey to cream, never saturated.
        assert(t[0] >= t[2] && t[0] - t[2] < 90);
    }
    std::puts("Saturn ring profile has the expected bands and gaps");
    return 0;
}
