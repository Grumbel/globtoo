#include "layer_composition.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>

// Country color and imagery are independent toggles. With both enabled,
// political fills are translucent and reveal the raster underneath.
int main() {
    constexpr float opaque = country_fill_opacity(false, true);
    constexpr float mixed = country_fill_opacity(true, true);
    static_assert(opaque == 1.0f);
    static_assert(mixed > 0.0f && mixed < 1.0f);
    static_assert(country_fill_opacity(true, false) == 1.0f);
    static_assert(country_fill_opacity(false, false) == 1.0f);

    // The two toggle states remain independent: neither forces the other off.
    bool countries = true, texture = true;
    const auto blend = [=](float country_color, float texture_color) {
        const float alpha = country_fill_opacity(texture, true);
        return countries ? country_color*alpha + texture_color*(1-alpha)
                         : texture_color;
    };
    assert(std::abs(blend(1.f,0.f)-mixed)<1e-6f);
    assert(std::abs(blend(0.f,1.f)-(1.f-mixed))<1e-6f);
    texture = false;
    assert(countries);
    assert(country_fill_opacity(texture, true)==1.f);
    countries = false;
    texture = true;
    assert(!countries && texture);

    std::puts("Independent country/texture toggles and overlay opacity: PASS");
}
