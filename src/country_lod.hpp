#pragma once

// Hysteresis avoids repeated mesh swaps when the zoom hovers at a threshold.
// The closer the camera is to radius=1, the greater the need for fine borders.
inline bool country_use_high_detail(float camera_distance, bool was_high,
                                    bool high_available, bool low_available) {
    if (!high_available) return false;
    if (!low_available) return true;
    return camera_distance < (was_high ? 2.15f : 1.85f);
}
