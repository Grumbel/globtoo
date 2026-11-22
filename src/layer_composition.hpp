#pragma once

// Country and imagery layers can be enabled independently. When both are
// present, retain enough underlying texture to reveal terrain and relief.
// No transparency is needed for a country-only political map, or when the
// texture couldn't be loaded.
inline constexpr float country_fill_opacity(bool texture_visible, bool texture_loaded) {
    return texture_visible && texture_loaded ? 0.60f : 1.0f;
}
