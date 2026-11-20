// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstddef>

#include "core/pad.h"

struct MouseSettings {
    float sensitivity = 1.0f;    // 0.05..10   1.0 = full tilt at 4 counts/ms
    float yRatio = 1.0f;         // 0.25..3    vertical relative to horizontal
    bool invertY = false;
    float exponent = 1.0f;       // 0.5..3     power curve
    float antiDeadzone = 0.12f;  // 0..0.5     jump past the game's own deadzone
    float outerLimit = 1.0f;     // 0.5..1     maximum tilt
    int windowMs = 8;            // 2..32      speed measurement window
    int recenterMs = 10;         // 2..100     centre after this long without motion

    void clamp();
};

constexpr int kMaxMouseEvent = 2000;  // a single delta larger than this is a glitch

// Mouse counts -> right-stick deflection. A stick sets turn *speed*, so the
// mouse speed over a short sliding window is mapped to deflection:
//   v   = counts in the last windowMs / windowMs        (counts per ms)
//   v.y *= yRatio; invertY flips it
//   d   = min(|v| * sensitivity / 4, 1) ^ exponent
//   out = d > 0 ? antiDeadzone + (outerLimit - antiDeadzone) * d : 0
//   result = direction(v) * out, length clamped to 1
// No motion for longer than recenterMs gives exactly the centre.
class MouseStick {
public:
    void configure(const MouseSettings& s);
    void addMotion(double tMs, int dx, int dy);
    Stick evaluate(double tMs);
    void reset();

    // Speed (counts/ms, after yRatio) -> deflection magnitude.
    static float shape(float speedCountsPerMs, const MouseSettings& s);

private:
    struct Sample {
        double t;
        int dx, dy;
    };
    void expire(double cutoff);

    static constexpr std::size_t kCapacity = 2048;
    MouseSettings settings_{};
    std::array<Sample, kCapacity> ring_{};
    std::size_t head_ = 0, count_ = 0;
    long long sumX_ = 0, sumY_ = 0;
    double lastMotion_ = -1e18;
};
