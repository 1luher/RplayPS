// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "core/profile.h"

namespace {
InputCode slot(const Profile& p, Action a, int i = 0) { return p.bindings.slots[static_cast<size_t>(a)][i]; }
}  // namespace

TEST_CASE("default_profile_has_gta_basics") {
    const Profile p = defaultGtaProfile();
    CHECK(slot(p, Action::MoveUp) == 'W');
    CHECK(slot(p, Action::R2) == code::MouseLeft);
    CHECK(slot(p, Action::L2) == code::MouseRight);
    CHECK(slot(p, Action::Square) == vk::Space);
    CHECK(slot(p, Action::Triangle, 1) == code::WheelUp);
}

TEST_CASE("default_profile_roundtrip") {
    Profile p = defaultGtaProfile();
    p.mouse.sensitivity = 2.5f;
    p.mouse.invertY = true;
    p.walkScale = 0.3f;
    std::vector<std::string> w;
    auto back = profileFromJson(profileToJson(p), &w);
    REQUIRE(back);
    CHECK(w.empty());
    CHECK(back->name == p.name);
    CHECK(back->bindings.slots == p.bindings.slots);
    CHECK(back->mouse.sensitivity == doctest::Approx(2.5f));
    CHECK(back->mouse.invertY);
    CHECK(back->mouse.windowMs == p.mouse.windowMs);
    CHECK(back->walkScale == doctest::Approx(0.3f));
}

TEST_CASE("missing_fields_take_defaults") {
    auto p = profileFromJson(R"({"name":"x"})", nullptr);
    REQUIRE(p);
    CHECK(p->name == "x");
    CHECK(p->mouse.sensitivity == 1.0f);
    CHECK(p->walkScale == 0.5f);
    CHECK(slot(*p, Action::MoveUp) == code::None);
}

TEST_CASE("unknown_key_is_skipped_with_warning") {
    std::vector<std::string> w;
    auto p = profileFromJson(R"({"bindings":{"cross":["Banana","Space"]}})", &w);
    REQUIRE(p);
    CHECK(slot(*p, Action::Cross) == vk::Space);
    REQUIRE(w.size() == 1);
    CHECK(w[0].find("Banana") != std::string::npos);
}

TEST_CASE("unknown_action_is_skipped_with_warning") {
    std::vector<std::string> w;
    auto p = profileFromJson(R"({"bindings":{"jump":["Space"]}})", &w);
    REQUIRE(p);
    REQUIRE(w.size() == 1);
    CHECK(w[0].find("jump") != std::string::npos);
}

TEST_CASE("out_of_range_values_are_clamped") {
    auto p = profileFromJson(R"({"walk_scale":5,"mouse":{"window_ms":500,"sensitivity":-3}})", nullptr);
    REQUIRE(p);
    CHECK(p->mouse.windowMs == 32);
    CHECK(p->mouse.sensitivity == 0.05f);
    CHECK(p->walkScale == 1.0f);
}

TEST_CASE("wrong_value_types_are_ignored_with_warning") {
    std::vector<std::string> w;
    auto p = profileFromJson(R"({"mouse":{"sensitivity":"fast"},"bindings":{"cross":"Space"}})", &w);
    REQUIRE(p);
    CHECK(p->mouse.sensitivity == 1.0f);
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
    auto p = profileFromJson(R"({"bindings":{"cross":["A","B","C"]}})", &w);
    REQUIRE(p);
    CHECK(slot(*p, Action::Cross, 0) == 'A');
    CHECK(slot(*p, Action::Cross, 1) == 'B');
    CHECK(w.size() == 1);
}
