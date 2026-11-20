// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include "ps5/link.h"

extern "C" {
#include <chiaki/controller.h>
}

TEST_CASE("neutral_pad_is_chiaki_idle") {
    ChiakiControllerState idle;
    chiaki_controller_state_set_idle(&idle);
    ChiakiControllerState s = padToChiaki(PadState{});
    CHECK(chiaki_controller_state_equals(&s, &idle));
}

TEST_CASE("buttons_map_to_chiaki_bits") {
    PadState p;
    p.set(PadButton::Cross, true);
    p.set(PadButton::Circle, true);
    p.set(PadButton::Create, true);
    p.set(PadButton::PS, true);
    const ChiakiControllerState s = padToChiaki(p);
    CHECK((s.buttons & CHIAKI_CONTROLLER_BUTTON_CROSS));
    CHECK((s.buttons & CHIAKI_CONTROLLER_BUTTON_MOON));
    CHECK((s.buttons & CHIAKI_CONTROLLER_BUTTON_SHARE));
    CHECK((s.buttons & CHIAKI_CONTROLLER_BUTTON_PS));
    CHECK_FALSE((s.buttons & CHIAKI_CONTROLLER_BUTTON_BOX));
}

TEST_CASE("triggers_and_sticks_scale_to_chiaki_ranges") {
    PadState p;
    p.l2 = 255;
    p.r2 = 128;
    p.left = {1.0f, -1.0f};
    p.right = {0.5f, 0.0f};
    const ChiakiControllerState s = padToChiaki(p);
    CHECK(s.l2_state == 255);
    CHECK(s.r2_state == 128);
    CHECK(s.left_x == 32767);
    CHECK(s.left_y == -32767);
    CHECK(s.right_x == 16384);
    CHECK(s.right_y == 0);
}

TEST_CASE("out_of_range_sticks_are_clamped") {
    PadState p;
    p.left = {3.0f, -7.0f};
    const ChiakiControllerState s = padToChiaki(p);
    CHECK(s.left_x == 32767);
    CHECK(s.left_y == -32767);
}
