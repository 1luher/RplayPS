// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/bindings.h"
#include "core/mouse_stick.h"

// How the player is moving right now; each class has its own layout.
enum class ControlClass : std::uint8_t { Foot, Ground, Water, Heli, Plane, Count };
constexpr std::size_t kControlClassCount = static_cast<std::size_t>(ControlClass::Count);
std::string_view controlClassName(ControlClass c);  // "foot", "ground", "water", "heli", "plane"
std::string_view controlClassTitle(ControlClass c);  // in the interface language
std::optional<ControlClass> parseControlClass(std::string_view name);

// Where mouse movement goes: the camera (right stick) or, in the air, pitch/roll (left stick).
enum class MouseTarget : std::uint8_t { RightStick, LeftStick };

struct Layout {
    Bindings bindings;
    MouseSettings mouse;
    MouseTarget mouseTarget = MouseTarget::RightStick;
    float walkScale = 0.5f;
    int steerRampMs = 0;       // 0..500: A/D reach full lock in this time (0 = instantly)
    float softTrigger = 0.4f;  // 0.1..1: L2/R2 strength while "soft" is held
};

struct Profile {
    std::string name = "GTA 6";
    std::array<Layout, kControlClassCount> layouts;
    Layout& layout(ControlClass c) { return layouts[static_cast<std::size_t>(c)]; }
    const Layout& layout(ControlClass c) const { return layouts[static_cast<std::size_t>(c)]; }
};

// Starting layouts per class, from the GTA 6 default control scheme (docs/gta6-controls.md).
Profile defaultGtaProfile();

std::string profileToJson(const Profile& p);
// Never throws. Unknown keys/actions/classes and wrong types are skipped with a warning (Russian);
// numbers are clamped. A class missing from the file gets its default layout. An old single-layout
// file (bindings/mouse at the top level) becomes the "foot" layout. Unparseable JSON -> nullopt.
std::optional<Profile> profileFromJson(std::string_view json, std::vector<std::string>* warnings);

// Controller state for a layout: key bindings from `b`, mouse stick to the right or left stick.
PadState composePad(const PadBuilder& b, MouseTarget target, Stick mouse, double tMs);

// One PadBuilder per class, all fed the same keys, so a key held while the class changes keeps
// working in the new layout. The active class builds the controller state.
class ClassPads {
public:
    explicit ClassPads(const Profile& p);
    void setInput(InputCode c, bool down, double tMs);
    void releaseAll();
    void setActive(ControlClass c) { active_ = c; }
    ControlClass active() const { return active_; }
    const Layout& activeLayout() const { return profile_.layout(active_); }
    PadState build(double tMs, Stick mouse) const;

private:
    Profile profile_;
    std::vector<PadBuilder> builders_;
    ControlClass active_ = ControlClass::Foot;
};
