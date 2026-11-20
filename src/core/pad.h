// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstdint>

enum class PadButton : std::uint8_t {
    Cross, Circle, Square, Triangle,
    DpadUp, DpadDown, DpadLeft, DpadRight,
    L1, R1, L3, R3,
    Options, Create, Touchpad, PS,
    Count
};

// Stick deflection in [-1, 1]; +y points down.
struct Stick {
    float x = 0.0f, y = 0.0f;
    bool operator==(const Stick&) const = default;
};

struct PadState {
    std::uint32_t buttons = 0;
    std::uint8_t l2 = 0, r2 = 0;
    Stick left, right;

    void set(PadButton b, bool on) {
        const std::uint32_t bit = 1u << static_cast<unsigned>(b);
        buttons = on ? (buttons | bit) : (buttons & ~bit);
    }
    bool pressed(PadButton b) const { return (buttons >> static_cast<unsigned>(b)) & 1u; }
    bool operator==(const PadState&) const = default;
};
