// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <cmath>

#include "core/driving.h"
#include "core/profile.h"

using doctest::Approx;

TEST_CASE("steering_ramps_towards_full_lock") {
    StickRamp r;
    r.setRampMs(100);  // full lock in 100 ms
    CHECK(r.step({1, 0}, 0).x == Approx(0.0f));
    CHECK(r.step({1, 0}, 50).x == Approx(0.5f));
    CHECK(r.step({1, 0}, 200).x == Approx(1.0f));
}

TEST_CASE("steering_returns_to_centre_with_the_same_ramp") {
    StickRamp r;
    r.setRampMs(100);
    r.step({1, 0}, 0);
    r.step({1, 0}, 100);
    CHECK(r.step({0, 0}, 150).x == Approx(0.5f));
    CHECK(r.step({0, 0}, 300).x == Approx(0.0f));
}

TEST_CASE("steering_ramp_zero_passes_through") {
    StickRamp r;
    r.setRampMs(0);
    CHECK(r.step({-1, 0.5f}, 0) == Stick{-1, 0.5f});
}

TEST_CASE("steering_ramp_only_smooths_x") {
    StickRamp r;
    r.setRampMs(100);
    r.step({0, 0}, 0);
    CHECK(r.step({0, -1}, 10).y == Approx(-1.0f));  // y (motorcycle lean) is not ramped
}

TEST_CASE("radial_aim_needs_a_hold") {
    RadialAim a;
    a.update(true, 0);
    a.addMotion(300, 0);
    CHECK_FALSE(a.active());
    CHECK(a.value() == Stick{});
    a.update(true, kRadialHoldMs + 1);
    CHECK(a.active());
}

TEST_CASE("radial_aim_points_where_the_mouse_went") {
    RadialAim a;
    a.update(true, 0);
    a.update(true, kRadialHoldMs + 1);
    a.addMotion(0, -300);  // up
    CHECK(a.value().y == Approx(-1.0f));
    CHECK(a.value().x == Approx(0.0f));
    a.addMotion(600, 200);  // accumulated motion is capped at kRadialRadius (200): net (600, 0) = right
    CHECK(a.value().x == Approx(1.0f));
}

TEST_CASE("radial_aim_ignores_small_motion_and_resets_on_release") {
    RadialAim a;
    a.update(true, 0);
    a.update(true, kRadialHoldMs + 1);
    a.addMotion(10, 5);
    CHECK(a.value() == Stick{});
    a.addMotion(300, 0);
    a.update(false, 400);
    CHECK_FALSE(a.active());
    CHECK(a.value() == Stick{});
}

TEST_CASE("soft_trigger_scales_l2_r2") {
    Bindings b;
    b.slots[static_cast<size_t>(Action::R2)][0] = 'W';
    b.slots[static_cast<size_t>(Action::Soft)][0] = vk::LAlt;
    PadBuilder pb(b, 0.5f, 0.4f);
    pb.setInput('W', true, 0);
    CHECK(pb.build(1, {}).r2 == 255);
    pb.setInput(vk::LAlt, true, 1);
    CHECK(pb.build(2, {}).r2 == 102);  // 0.4 * 255
}

TEST_CASE("driving_settings_roundtrip_in_profile") {
    Profile p = defaultGtaProfile();
    CHECK(p.layout(ControlClass::Ground).steerRampMs > 0);
    CHECK(p.layout(ControlClass::Foot).steerRampMs == 0);
    p.layout(ControlClass::Ground).steerRampMs = 222;
    p.layout(ControlClass::Ground).softTrigger = 0.25f;
    auto back = profileFromJson(profileToJson(p), nullptr);
    REQUIRE(back);
    CHECK(back->layout(ControlClass::Ground).steerRampMs == 222);
    CHECK(back->layout(ControlClass::Ground).softTrigger == Approx(0.25f));
}
