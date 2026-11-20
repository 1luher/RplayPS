// SPDX-License-Identifier: AGPL-3.0-only
#include "core/mouse_stick.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

void MouseSettings::clamp() {
    sensitivity = std::clamp(sensitivity, 0.05f, 10.0f);
    yRatio = std::clamp(yRatio, 0.25f, 3.0f);
    exponent = std::clamp(exponent, 0.5f, 3.0f);
    antiDeadzone = std::clamp(antiDeadzone, 0.0f, 0.5f);
    outerLimit = std::clamp(outerLimit, 0.5f, 1.0f);
    windowMs = std::clamp(windowMs, 2, 32);
    recenterMs = std::clamp(recenterMs, 2, 100);
}

void MouseStick::configure(const MouseSettings& s) {
    settings_ = s;
    settings_.clamp();
}

void MouseStick::reset() {
    head_ = count_ = 0;
    sumX_ = sumY_ = 0;
    lastMotion_ = -1e18;
}

void MouseStick::addMotion(double tMs, int dx, int dy) {
    if (dx == 0 && dy == 0) return;
    if (std::abs(dx) > kMaxMouseEvent || std::abs(dy) > kMaxMouseEvent) return;
    if (count_ == kCapacity) {  // full: drop the oldest sample
        sumX_ -= ring_[head_].dx;
        sumY_ -= ring_[head_].dy;
        head_ = (head_ + 1) % kCapacity;
        --count_;
    }
    ring_[(head_ + count_) % kCapacity] = {tMs, dx, dy};
    ++count_;
    sumX_ += dx;
    sumY_ += dy;
    lastMotion_ = tMs;
}

void MouseStick::expire(double cutoff) {
    while (count_ > 0 && ring_[head_].t <= cutoff) {
        sumX_ -= ring_[head_].dx;
        sumY_ -= ring_[head_].dy;
        head_ = (head_ + 1) % kCapacity;
        --count_;
    }
}

float MouseStick::shape(float speedCountsPerMs, const MouseSettings& s) {
    if (speedCountsPerMs <= 0.0f) return 0.0f;
    float d = std::min(speedCountsPerMs * s.sensitivity / 4.0f, 1.0f);
    if (s.exponent != 1.0f) d = std::pow(d, s.exponent);
    if (d <= 0.0f) return 0.0f;
    return s.antiDeadzone + (s.outerLimit - s.antiDeadzone) * d;
}

Stick MouseStick::evaluate(double tMs) {
    const double window = settings_.windowMs;
    expire(tMs - window);
    if (count_ == 0 || tMs - lastMotion_ > settings_.recenterMs) return {};

    const float vx = static_cast<float>(sumX_ / window);
    float vy = static_cast<float>(sumY_ / window) * settings_.yRatio;
    if (settings_.invertY) vy = -vy;
    const float speed = std::sqrt(vx * vx + vy * vy);
    if (speed <= 0.0f) return {};

    const float d = shape(speed, settings_);
    Stick out{vx / speed * d, vy / speed * d};
    const float len = std::sqrt(out.x * out.x + out.y * out.y);
    if (len > 1.0f) {
        out.x /= len;
        out.y /= len;
    }
    return out;
}
