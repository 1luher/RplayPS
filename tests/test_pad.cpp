// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include "core/pad.h"

TEST_CASE("pad_set_and_pressed") {
    PadState p;
    p.set(PadButton::Cross, true);
    CHECK(p.pressed(PadButton::Cross));
    CHECK_FALSE(p.pressed(PadButton::Circle));
    p.set(PadButton::Cross, false);
    CHECK(p == PadState{});
}

TEST_CASE("pad_buttons_are_independent") {
    PadState p;
    p.set(PadButton::PS, true);
    p.set(PadButton::Square, true);
    p.set(PadButton::PS, false);
    CHECK(p.pressed(PadButton::Square));
    CHECK_FALSE(p.pressed(PadButton::PS));
}
