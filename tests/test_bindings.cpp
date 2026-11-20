// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <cmath>

#include "core/bindings.h"

using doctest::Approx;

namespace {

PadBuilder makeBuilder(float walkScale = 0.5f) {
    Bindings b;
    auto bind = [&](Action a, InputCode c, int slot = 0) { b.slots[static_cast<size_t>(a)][slot] = c; };
    bind(Action::MoveUp, 'W');
    bind(Action::MoveLeft, 'A');
    bind(Action::MoveDown, 'S');
    bind(Action::MoveRight, 'D');
    bind(Action::Walk, vk::LAlt);
    bind(Action::R2, code::MouseLeft);
    bind(Action::Square, vk::Space);
    bind(Action::Triangle, 'F');
    bind(Action::Triangle, code::WheelUp, 1);
    return PadBuilder(b, walkScale);
}

}  // namespace

TEST_CASE("w_moves_up") {
    auto pb = makeBuilder();
    pb.setInput('W', true, 0);
    CHECK(pb.build(1, {}).left.y == -1.0f);
}

TEST_CASE("opposite_keys_last_wins") {
    auto pb = makeBuilder();
    pb.setInput('W', true, 0);
    pb.setInput('S', true, 5);
    CHECK(pb.build(6, {}).left.y == 1.0f);
    pb.setInput('S', false, 7);
    CHECK(pb.build(8, {}).left.y == -1.0f);
}

TEST_CASE("diagonal_is_normalised") {
    auto pb = makeBuilder();
    pb.setInput('W', true, 0);
    pb.setInput('A', true, 0);
    const Stick l = pb.build(1, {}).left;
    CHECK(std::hypot(l.x, l.y) == Approx(1.0f));
    CHECK(l.x < 0.0f);
}

TEST_CASE("walk_scales_left_stick") {
    auto pb = makeBuilder(0.5f);
    pb.setInput('W', true, 0);
    pb.setInput(vk::LAlt, true, 0);
    CHECK(pb.build(1, {}).left.y == Approx(-0.5f));
}

TEST_CASE("mouse_left_is_full_r2") {
    auto pb = makeBuilder();
    pb.setInput(code::MouseLeft, true, 0);
    CHECK(pb.build(1, {}).r2 == 255);
    pb.setInput(code::MouseLeft, false, 2);
    CHECK(pb.build(3, {}).r2 == 0);
}

TEST_CASE("button_from_key") {
    auto pb = makeBuilder();
    pb.setInput(vk::Space, true, 0);
    CHECK(pb.build(1, {}).pressed(PadButton::Square));
}

TEST_CASE("two_keys_on_one_action") {
    auto pb = makeBuilder();
    pb.setInput('F', true, 0);
    pb.setInput(code::WheelUp, true, 0);
    pb.setInput('F', false, 1);
    CHECK(pb.build(2, {}).pressed(PadButton::Triangle));  // wheel pulse still active
}

TEST_CASE("wheel_is_a_60ms_pulse") {
    auto pb = makeBuilder();
    pb.setInput(code::WheelUp, true, 100);
    CHECK(pb.build(159, {}).pressed(PadButton::Triangle));
    CHECK_FALSE(pb.build(161, {}).pressed(PadButton::Triangle));
}

TEST_CASE("right_stick_passes_through") {
    auto pb = makeBuilder();
    CHECK(pb.build(1, {0.3f, -0.2f}).right == Stick{0.3f, -0.2f});
}

TEST_CASE("unbound_key_does_nothing") {
    auto pb = makeBuilder();
    pb.setInput('Z', true, 0);
    CHECK(pb.build(1, {}) == PadState{});
}

TEST_CASE("release_all_gives_neutral_pad") {
    auto pb = makeBuilder();
    pb.setInput('W', true, 0);
    pb.setInput(vk::Space, true, 0);
    pb.setInput(code::WheelUp, true, 0);
    pb.releaseAll();
    CHECK(pb.build(1, {}) == PadState{});
}

TEST_CASE("code_names_roundtrip") {
    for (InputCode c : {InputCode('W'), InputCode('7'), vk::Space, vk::LShift, vk::Esc, vk::Up, InputCode(0x74),
                        code::MouseRight, code::MouseX2, code::WheelDown})
        CHECK(parseInputCode(inputCodeName(c)) == c);
    CHECK(inputCodeName(vk::LShift) == "LShift");
    CHECK(inputCodeName(code::MouseLeft) == "Mouse1");
    CHECK_FALSE(parseInputCode("F8"));
    CHECK_FALSE(parseInputCode("Banana"));
    CHECK_FALSE(parseInputCode(""));
}

TEST_CASE("action_names_roundtrip") {
    for (size_t i = 0; i < static_cast<size_t>(Action::Count); ++i) {
        const auto a = static_cast<Action>(i);
        CHECK(parseAction(actionName(a)) == a);
    }
    CHECK(actionName(Action::MoveUp) == "move_up");
    CHECK(actionName(Action::L2) == "l2");
    CHECK_FALSE(parseAction("jump"));
}
