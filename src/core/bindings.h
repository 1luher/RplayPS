// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "core/pad.h"

// Everything a key can drive. The first 16 entries map 1:1 onto PadButton.
enum class Action : std::uint8_t {
    Cross, Circle, Square, Triangle,
    DpadUp, DpadDown, DpadLeft, DpadRight,
    L1, R1, L3, R3,
    Options, Create, Touchpad, PS,
    L2, R2,
    MoveUp, MoveDown, MoveLeft, MoveRight,
    Walk,
    Count
};
constexpr std::size_t kActionCount = static_cast<std::size_t>(Action::Count);

// 1..255 = Windows virtual-key code (left/right Shift/Ctrl/Alt kept apart), 0x101.. = mouse.
using InputCode = std::uint16_t;
namespace code {
constexpr InputCode None = 0, MouseLeft = 0x101, MouseRight = 0x102, MouseMiddle = 0x103, MouseX1 = 0x104,
                    MouseX2 = 0x105, WheelUp = 0x106, WheelDown = 0x107, Max = 0x108;
}
// Windows VK values, so rplay_core and its tests do not depend on windows.h.
namespace vk {
constexpr InputCode Backspace = 0x08, Tab = 0x09, Enter = 0x0D, Esc = 0x1B, Space = 0x20, Left = 0x25,
                    Up = 0x26, Right = 0x27, Down = 0x28, F1 = 0x70, F8 = 0x77, F12 = 0x7B, LShift = 0xA0,
                    RShift = 0xA1, LCtrl = 0xA2, RCtrl = 0xA3, LAlt = 0xA4, RAlt = 0xA5;
}

struct Bindings {
    std::array<std::array<InputCode, 2>, kActionCount> slots{};
};

std::string inputCodeName(InputCode c);  // "W", "Space", "LShift", "Mouse1", "WheelUp", "F5"; "" if unknown
std::optional<InputCode> parseInputCode(std::string_view name);  // F8 is reserved: nullopt
std::string_view actionName(Action a);   // "cross", "l2", "move_up", "walk"
std::optional<Action> parseAction(std::string_view name);

constexpr double kWheelPulseMs = 60.0;

// Held inputs -> controller state. Opposite directions: the most recently pressed wins.
class PadBuilder {
public:
    PadBuilder(const Bindings& b, float walkScale);  // walkScale is clamped to 0.1..1
    void setInput(InputCode c, bool down, double tMs);
    void releaseAll();
    PadState build(double tMs, Stick right) const;

private:
    bool codeActive(InputCode c, double tMs) const;
    bool actionActive(Action a, double tMs) const;
    double lastPress(Action a, double tMs) const;

    Bindings bindings_;
    float walkScale_;
    std::array<bool, code::Max> down_{};
    std::array<double, code::Max> pressTime_{};
    double wheelUntil_[2] = {-1e18, -1e18};
};
