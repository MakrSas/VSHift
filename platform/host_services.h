#pragma once

#include <cstdint>

namespace vshift::platform {

// Host-facing input state. Touch controls and physical controllers are
// normalized by the frontend before a console core receives this structure.
struct ControllerState final {
    bool dpad_up = false;
    bool dpad_down = false;
    bool dpad_left = false;
    bool dpad_right = false;
    bool left_stick_click = false;
    bool right_stick_click = false;
    float left_stick_x = 0.0f;
    float left_stick_y = 0.0f;
    float right_stick_x = 0.0f;
    float right_stick_y = 0.0f;
    bool cross = false;
    bool circle = false;
    bool square = false;
    bool triangle = false;
    bool l1 = false;
    bool l2 = false;
    bool r1 = false;
    bool r2 = false;
    bool start = false;
    bool select = false;
    bool ps = false;
};

struct PowerState final {
    std::uint8_t battery_level = 100;
    bool charging = false;
    bool external_power = true;
    bool battery_present = true;
};

} // namespace vshift::platform
