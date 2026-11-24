// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bodies.hpp"
#include <vector>

// Radial structure of a planetary ring system, sampled into an RGBA strip:
// texel i covers radius inner_km + (i + 0.5) / width * (outer_km - inner_km).
// RGB is the ring particle color; alpha is the normal-incidence opacity
// 1 - exp(-tau). Generated procedurally from published ring and gap radii,
// so no third-party ring imagery is needed.
struct RingProfile {
    double inner_km = 0, outer_km = 0; // radial extent of the strip
    int width = 0;
    std::vector<unsigned char> rgba;   // width * 4 bytes
    bool valid() const { return width > 0 && outer_km > inner_km; }
};

// Rings with optically visible structure. Currently only Saturn; other
// bodies return an empty profile.
RingProfile ring_profile(BodyId body, int width = 2048);
// Opacity at normal incidence of the sampled profile at a radius (tests/QA).
double ring_profile_opacity(const RingProfile& profile, double radius_km);
