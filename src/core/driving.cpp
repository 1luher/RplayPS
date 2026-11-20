// SPDX-License-Identifier: AGPL-3.0-only
#include "core/driving.h"

#include <algorithm>
#include <cmath>

Stick StickRamp::step(Stick target, double tMs) {
    if (rampMs_ <= 0) {
        x_ = target.x;
        last_ = tMs;
        return target;
    }
    const double dt = last_ < 0 ? 0 : std::max(0.0, tMs - last_);
    last_ = tMs;
    const float maxStep = static_cast<float>(dt / rampMs_);
    x_ += std::clamp(target.x - x_, -maxStep, maxStep);
    return {x_, target.y};
}

void RadialAim::update(bool menuHeld, double tMs) {
    if (!menuHeld) {
        held_ = active_ = false;
        ax_ = ay_ = 0;
        return;
    }
    if (!held_) {
        held_ = true;
        heldSince_ = tMs;
        ax_ = ay_ = 0;
    }
    if (!active_ && tMs - heldSince_ >= kRadialHoldMs) active_ = true;
}

void RadialAim::addMotion(int dx, int dy) {
    if (!active_) return;
    ax_ += static_cast<float>(dx);
    ay_ += static_cast<float>(dy);
    const float len = std::hypot(ax_, ay_);
    if (len > kRadialRadius) {
        ax_ *= kRadialRadius / len;
        ay_ *= kRadialRadius / len;
    }
}

Stick RadialAim::value() const {
    const float len = std::hypot(ax_, ay_);
    if (!active_ || len < kRadialDeadzone) return {};
    return {ax_ / len, ay_ / len};
}
