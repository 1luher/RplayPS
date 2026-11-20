// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <cmath>

#include "core/mouse_stick.h"

using doctest::Approx;

TEST_CASE("no_motion_is_centre") {
    MouseStick m;
    m.configure({});
    CHECK(m.evaluate(100) == Stick{});
}

TEST_CASE("one_count_per_ms_right") {
    MouseStick m;
    m.configure({});
    for (int t = 1; t <= 8; ++t) m.addMotion(t, 1, 0);
    const Stick s = m.evaluate(8);
    CHECK(s.x == Approx(0.12f + 0.88f * 0.25f));
    CHECK(s.y == 0.0f);
}

TEST_CASE("old_motion_leaves_the_window") {
    MouseStick m;
    MouseSettings s;
    s.recenterMs = 100;
    m.configure(s);
    m.addMotion(1, 8, 0);   // falls out of the 8 ms window by t = 9
    m.addMotion(9, 8, 0);
    CHECK(m.evaluate(9).x == Approx(MouseStick::shape(1.0f, s)));
}

TEST_CASE("fast_motion_clamps_to_outer_limit") {
    MouseStick m;
    MouseSettings s;
    s.outerLimit = 0.9f;
    m.configure(s);
    m.addMotion(8, 400, 0);
    CHECK(m.evaluate(8).x == Approx(0.9f));
}

TEST_CASE("exponent_two") {
    MouseSettings s;
    s.exponent = 2.0f;
    CHECK(MouseStick::shape(1.0f, s) == Approx(0.12f + 0.88f * 0.0625f));
}

TEST_CASE("y_ratio_and_invert") {
    MouseStick m;
    MouseSettings s;
    s.yRatio = 2.0f;
    s.invertY = true;
    m.configure(s);
    m.addMotion(8, 0, 4);  // 4 counts / 8 ms * 2 = 1 count/ms
    const Stick r = m.evaluate(8);
    CHECK(r.y < 0.0f);
    CHECK(r.x == 0.0f);
    CHECK(-r.y == Approx(MouseStick::shape(1.0f, s)));
}

TEST_CASE("diagonal_length_never_exceeds_one") {
    MouseStick m;
    m.configure({});
    m.addMotion(8, 900, 900);
    const Stick r = m.evaluate(8);
    CHECK(std::hypot(r.x, r.y) <= 1.0001f);
    CHECK(r.x == Approx(r.y));
}

TEST_CASE("recentres_after_recenter_ms") {
    MouseStick m;
    m.configure({});
    m.addMotion(8, 8, 0);
    CHECK(m.evaluate(8).x > 0.0f);
    CHECK(m.evaluate(8 + 11) == Stick{});
}

TEST_CASE("huge_single_delta_is_ignored") {
    MouseStick m;
    m.configure({});
    m.addMotion(8, 5000, 0);
    CHECK(m.evaluate(8) == Stick{});
}

TEST_CASE("reset_clears_motion") {
    MouseStick m;
    m.configure({});
    m.addMotion(8, 8, 0);
    m.reset();
    CHECK(m.evaluate(8) == Stick{});
}

TEST_CASE("settings_clamp") {
    MouseSettings s;
    s.windowMs = 0;
    s.sensitivity = 99.0f;
    s.antiDeadzone = 0.9f;
    s.clamp();
    CHECK(s.windowMs == 2);
    CHECK(s.sensitivity == 10.0f);
    CHECK(s.antiDeadzone == 0.5f);
}
