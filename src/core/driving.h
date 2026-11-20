// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "core/pad.h"

// Smooth steering: the stick's X moves towards the keys' target at 1/rampMs per ms, so A/D steer
// progressively instead of jumping to full lock. Y passes through (motorcycle lean). 0 = off.
class StickRamp {
public:
    void setRampMs(int ms) { rampMs_ = ms; }
    Stick step(Stick target, double tMs);

private:
    int rampMs_ = 0;
    float x_ = 0;
    double last_ = -1;
};

// Radial menus (weapon wheel, vehicle menu, partner menu): while the menu button is held, the
// mouse points the right stick in a direction instead of turning the camera.
constexpr double kRadialHoldMs = 250;  // a tap is a tap; only a hold opens the wheel
constexpr float kRadialRadius = 200;   // counts: accumulated motion is capped here
constexpr float kRadialDeadzone = 40;  // counts: smaller motion selects nothing

class RadialAim {
public:
    void update(bool menuHeld, double tMs);
    bool active() const { return active_; }
    void addMotion(int dx, int dy);
    Stick value() const;

private:
    bool held_ = false, active_ = false;
    double heldSince_ = 0;
    float ax_ = 0, ay_ = 0;
};
