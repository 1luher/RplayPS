// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/profile.h"

namespace {
const Layout& L(const Profile& p, ControlClass c) { return p.layouts[static_cast<size_t>(c)]; }
InputCode slot(const Profile& p, ControlClass c, Action a, int i = 0) {
    return L(p, c).bindings.slots[static_cast<size_t>(a)][i];
}
std::optional<Profile> fromFoot(const std::string& footJson, std::vector<std::string>* w) {
    return profileFromJson(R"({"layouts":{"foot":)" + footJson + "}}", w);
}
}  // namespace

TEST_CASE("default_profile_has_a_layout_per_class") {
    const Profile p = defaultGtaProfile();
    CHECK(slot(p, ControlClass::Foot, Action::MoveUp) == 'W');
    CHECK(slot(p, ControlClass::Foot, Action::R2) == code::MouseLeft);
    CHECK(slot(p, ControlClass::Foot, Action::L2) == code::MouseRight);
    CHECK(slot(p, ControlClass::Foot, Action::Cross) == vk::Space);
    CHECK(slot(p, ControlClass::Foot, Action::L3) == vk::LShift);
    CHECK(L(p, ControlClass::Foot).mouseTarget == MouseTarget::RightStick);

    CHECK(slot(p, ControlClass::Ground, Action::R2) == 'W');
    CHECK(slot(p, ControlClass::Ground, Action::L2) == 'S');
    CHECK(slot(p, ControlClass::Ground, Action::MoveLeft) == 'A');
    CHECK(slot(p, ControlClass::Ground, Action::Cross) == vk::Space);
    CHECK(slot(p, ControlClass::Water, Action::R2) == 'W');

    CHECK(L(p, ControlClass::Heli).mouseTarget == MouseTarget::LeftStick);
    CHECK(slot(p, ControlClass::Heli, Action::R2) == 'W');
    CHECK(slot(p, ControlClass::Heli, Action::Square) == 'A');
    CHECK(L(p, ControlClass::Plane).mouseTarget == MouseTarget::LeftStick);
    CHECK(slot(p, ControlClass::Plane, Action::Circle) == 'D');
}

TEST_CASE("class_names_roundtrip") {
    for (size_t i = 0; i < kControlClassCount; ++i) {
        const auto c = static_cast<ControlClass>(i);
        CHECK(parseControlClass(controlClassName(c)) == c);
    }
    CHECK(controlClassName(ControlClass::Heli) == "heli");
    CHECK_FALSE(parseControlClass("submarine"));
}

TEST_CASE("profile_roundtrip_keeps_every_class") {
    Profile p = defaultGtaProfile();
    p.layouts[0].mouse.sensitivity = 2.5f;
    p.layouts[0].mouse.invertY = true;
    p.layouts[0].walkScale = 0.3f;
    p.layouts[3].mouse.sensitivity = 0.7f;
    std::vector<std::string> w;
    auto back = profileFromJson(profileToJson(p), &w);
    REQUIRE(back);
    CHECK(w.empty());
    CHECK(back->name == p.name);
    for (size_t i = 0; i < kControlClassCount; ++i) {
        CHECK(back->layouts[i].bindings.slots == p.layouts[i].bindings.slots);
        CHECK(back->layouts[i].mouseTarget == p.layouts[i].mouseTarget);
    }
    CHECK(back->layouts[0].mouse.sensitivity == doctest::Approx(2.5f));
    CHECK(back->layouts[0].mouse.invertY);
    CHECK(back->layouts[0].walkScale == doctest::Approx(0.3f));
    CHECK(back->layouts[3].mouse.sensitivity == doctest::Approx(0.7f));
}

TEST_CASE("missing_class_takes_its_default_layout") {
    auto p = profileFromJson(R"({"name":"x","layouts":{}})", nullptr);
    REQUIRE(p);
    CHECK(p->name == "x");
    CHECK(slot(*p, ControlClass::Ground, Action::R2) == 'W');
    CHECK(slot(*p, ControlClass::Foot, Action::MoveUp) == 'W');
}

TEST_CASE("legacy_single_layout_file_becomes_foot") {
    std::vector<std::string> w;
    auto p = profileFromJson(R"({"walk_scale":0.4,"mouse":{"sensitivity":2},"bindings":{"cross":["Space"]}})", &w);
    REQUIRE(p);
    CHECK(slot(*p, ControlClass::Foot, Action::Cross) == vk::Space);
    CHECK(slot(*p, ControlClass::Foot, Action::MoveUp) == code::None);  // legacy file defines foot fully
    CHECK(L(*p, ControlClass::Foot).mouse.sensitivity == 2.0f);
    CHECK(L(*p, ControlClass::Foot).walkScale == doctest::Approx(0.4f));
    CHECK(slot(*p, ControlClass::Ground, Action::R2) == 'W');  // other classes: defaults
}

TEST_CASE("unknown_key_is_skipped_with_warning") {
    std::vector<std::string> w;
    auto p = fromFoot(R"({"bindings":{"cross":["Banana","Space"]}})", &w);
    REQUIRE(p);
    CHECK(slot(*p, ControlClass::Foot, Action::Cross) == vk::Space);
    REQUIRE(w.size() == 1);
    CHECK(w[0].find("Banana") != std::string::npos);
}

TEST_CASE("unknown_action_and_class_are_skipped_with_warning") {
    std::vector<std::string> w;
    auto p = profileFromJson(R"({"layouts":{"foot":{"bindings":{"jump":["Space"]}},"submarine":{}}})", &w);
    REQUIRE(p);
    REQUIRE(w.size() == 2);
    CHECK(w[0].find("jump") != std::string::npos);
    CHECK(w[1].find("submarine") != std::string::npos);
}

TEST_CASE("bad_mouse_target_keeps_default_with_warning") {
    std::vector<std::string> w;
    auto p = profileFromJson(R"({"layouts":{"plane":{"mouse_target":"tail"}}})", &w);
    REQUIRE(p);
    CHECK(L(*p, ControlClass::Plane).mouseTarget == MouseTarget::LeftStick);
    CHECK(w.size() == 1);
}

TEST_CASE("out_of_range_values_are_clamped") {
    auto p = fromFoot(R"({"walk_scale":5,"mouse":{"window_ms":500,"sensitivity":-3}})", nullptr);
    REQUIRE(p);
    CHECK(L(*p, ControlClass::Foot).mouse.windowMs == 32);
    CHECK(L(*p, ControlClass::Foot).mouse.sensitivity == 0.05f);
    CHECK(L(*p, ControlClass::Foot).walkScale == 1.0f);
}

TEST_CASE("wrong_value_types_are_ignored_with_warning") {
    std::vector<std::string> w;
    auto p = fromFoot(R"({"mouse":{"sensitivity":"fast"},"bindings":{"cross":"Space"}})", &w);
    REQUIRE(p);
    CHECK(L(*p, ControlClass::Foot).mouse.sensitivity == 1.0f);
    CHECK(w.size() == 2);
}

TEST_CASE("broken_json_returns_nullopt") {
    std::vector<std::string> w;
    CHECK_FALSE(profileFromJson("{oops", &w));
    CHECK_FALSE(w.empty());
    CHECK_FALSE(profileFromJson("[1,2]", &w));
}

TEST_CASE("more_than_two_keys_keeps_first_two") {
    std::vector<std::string> w;
    auto p = fromFoot(R"({"bindings":{"cross":["A","B","C"]}})", &w);
    REQUIRE(p);
    CHECK(slot(*p, ControlClass::Foot, Action::Cross, 0) == 'A');
    CHECK(slot(*p, ControlClass::Foot, Action::Cross, 1) == 'B');
    CHECK(w.size() == 1);
}

TEST_CASE("profile_with_huge_function_key_loads") {
    std::vector<std::string> w;
    std::optional<Profile> p;
    CHECK_NOTHROW(p = fromFoot(R"({"bindings":{"cross":["F99999999999","Space"]}})", &w));
    REQUIRE(p);
    CHECK(slot(*p, ControlClass::Foot, Action::Cross) == vk::Space);
}

TEST_CASE("mouse_drives_right_stick_on_foot") {
    const Profile p = defaultGtaProfile();
    PadBuilder b(p.layout(ControlClass::Foot).bindings, 0.5f);
    const PadState s = composePad(b, MouseTarget::RightStick, {0.4f, -0.2f}, 1);
    CHECK(s.right == Stick{0.4f, -0.2f});
    CHECK(s.left == Stick{});
}

TEST_CASE("mouse_drives_left_stick_in_the_air") {
    const Profile p = defaultGtaProfile();
    PadBuilder b(p.layout(ControlClass::Plane).bindings, 0.5f);
    b.setInput('W', true, 0);
    const PadState s = composePad(b, MouseTarget::LeftStick, {0.5f, 0.1f}, 1);
    CHECK(s.left == Stick{0.5f, 0.1f});
    CHECK(s.right == Stick{});
    CHECK(s.r2 == 255);  // W = throttle in the plane layout
}

TEST_CASE("held_key_carries_over_when_the_class_changes") {
    ClassPads pads(defaultGtaProfile());
    pads.setInput('W', true, 0);  // walking forward...
    CHECK(pads.build(1, {}).left.y == -1.0f);
    pads.setActive(ControlClass::Ground);  // ...gets into a car with W still held
    const PadState s = pads.build(2, {});
    CHECK(s.r2 == 255);
    CHECK(s.left == Stick{});
    CHECK(pads.active() == ControlClass::Ground);
}

TEST_CASE("class_pads_route_mouse_by_active_layout") {
    ClassPads pads(defaultGtaProfile());
    pads.setActive(ControlClass::Heli);
    CHECK(pads.build(1, {0.3f, 0}).left == Stick{0.3f, 0});
    pads.setActive(ControlClass::Foot);
    CHECK(pads.build(2, {0.3f, 0}).right == Stick{0.3f, 0});
}

TEST_CASE("class_pads_release_all") {
    ClassPads pads(defaultGtaProfile());
    pads.setInput('W', true, 0);
    pads.releaseAll();
    pads.setActive(ControlClass::Ground);
    CHECK(pads.build(1, {}) == PadState{});
}
