// SPDX-License-Identifier: AGPL-3.0-only
#include "core/bindings.h"

#include <algorithm>
#include <cmath>

namespace {

struct NamedCode {
    std::string_view name;
    InputCode code;
};

constexpr NamedCode kNamed[] = {
    {"Space", vk::Space},       {"Enter", vk::Enter},         {"Esc", vk::Esc},
    {"Tab", vk::Tab},           {"Backspace", vk::Backspace}, {"LShift", vk::LShift},
    {"RShift", vk::RShift},     {"LCtrl", vk::LCtrl},         {"RCtrl", vk::RCtrl},
    {"LAlt", vk::LAlt},         {"RAlt", vk::RAlt},           {"Up", vk::Up},
    {"Down", vk::Down},         {"Left", vk::Left},           {"Right", vk::Right},
    {"Mouse1", code::MouseLeft}, {"Mouse2", code::MouseRight}, {"Mouse3", code::MouseMiddle},
    {"Mouse4", code::MouseX1},  {"Mouse5", code::MouseX2},    {"WheelUp", code::WheelUp},
    {"WheelDown", code::WheelDown}, {"Home", vk::Home},         {"End", vk::End},
    {"Insert", vk::Insert},    {"PageUp", vk::PageUp},       {"PageDown", vk::PageDown},
};

constexpr std::string_view kActionNames[kActionCount] = {
    "cross",    "circle",    "square",    "triangle",   "dpad_up", "dpad_down", "dpad_left", "dpad_right",
    "l1",       "r1",        "l3",        "r3",         "options", "create",    "touchpad",  "ps",
    "l2",       "r2",        "move_up",   "move_down",  "move_left", "move_right", "walk",     "soft",
};

}  // namespace

InputCode inputCodeFromRawKey(unsigned vkey, unsigned makeCode, bool e0) {
    constexpr unsigned kShift = 0x10, kControl = 0x11, kMenu = 0x12;
    if (vkey == 0 || vkey >= 0xFF) return code::None;  // 0xFF: fake keys of escape sequences
    switch (vkey) {
        case kShift: return makeCode == 0x36 ? vk::RShift : vk::LShift;
        case kControl: return e0 ? vk::RCtrl : vk::LCtrl;
        case kMenu: return e0 ? vk::RAlt : vk::LAlt;
        default: return static_cast<InputCode>(vkey);
    }
}

std::string inputCodeName(InputCode c) {
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return std::string(1, static_cast<char>(c));
    if (c >= vk::F1 && c <= vk::F12) return "F" + std::to_string(c - vk::F1 + 1);
    for (const auto& n : kNamed)
        if (n.code == c) return std::string(n.name);
    return {};
}

std::optional<InputCode> parseInputCode(std::string_view name) {
    if (name.size() == 1) {
        const char ch = name[0];
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) return static_cast<InputCode>(ch);
        if (ch >= 'a' && ch <= 'z') return static_cast<InputCode>(ch - 'a' + 'A');
        return std::nullopt;
    }
    if ((name.size() == 2 || name.size() == 3) && name[0] == 'F' &&
        std::all_of(name.begin() + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        int n = 0;  // at most two digits: no overflow, no exceptions
        for (char c : name.substr(1)) n = n * 10 + (c - '0');
        if (n < 1 || n > 12 || n == 8) return std::nullopt;  // F8 toggles capture
        return static_cast<InputCode>(vk::F1 + n - 1);
    }
    for (const auto& n : kNamed)
        if (n.name == name) return n.code;
    return std::nullopt;
}

std::string_view actionName(Action a) {
    const auto i = static_cast<std::size_t>(a);
    return i < kActionCount ? kActionNames[i] : std::string_view{};
}

std::optional<Action> parseAction(std::string_view name) {
    for (std::size_t i = 0; i < kActionCount; ++i)
        if (kActionNames[i] == name) return static_cast<Action>(i);
    return std::nullopt;
}

PadBuilder::PadBuilder(const Bindings& b, float walkScale, float softTrigger)
    : bindings_(b), walkScale_(std::clamp(walkScale, 0.1f, 1.0f)), softTrigger_(std::clamp(softTrigger, 0.1f, 1.0f)) {}

void PadBuilder::setInput(InputCode c, bool down, double tMs) {
    if (c == code::None || c >= code::Max) return;
    if (c == code::WheelUp || c == code::WheelDown) {
        if (down) {
            wheelUntil_[c == code::WheelUp ? 0 : 1] = tMs + kWheelPulseMs;
            pressTime_[c] = tMs;
        }
        return;
    }
    if (down && !down_[c]) pressTime_[c] = tMs;
    down_[c] = down;
}

void PadBuilder::releaseAll() {
    down_.fill(false);
    wheelUntil_[0] = wheelUntil_[1] = -1e18;
}

bool PadBuilder::codeActive(InputCode c, double tMs) const {
    if (c == code::None || c >= code::Max) return false;
    if (c == code::WheelUp) return tMs < wheelUntil_[0];
    if (c == code::WheelDown) return tMs < wheelUntil_[1];
    return down_[c];
}

bool PadBuilder::actionActive(Action a, double tMs) const {
    for (InputCode c : bindings_.slots[static_cast<std::size_t>(a)])
        if (codeActive(c, tMs)) return true;
    return false;
}

double PadBuilder::lastPress(Action a, double tMs) const {
    double t = -1e18;
    for (InputCode c : bindings_.slots[static_cast<std::size_t>(a)])
        if (codeActive(c, tMs)) t = std::max(t, pressTime_[c]);
    return t;
}

PadState PadBuilder::build(double tMs, Stick right) const {
    PadState pad;
    for (std::size_t i = 0; i < static_cast<std::size_t>(PadButton::Count); ++i)
        pad.set(static_cast<PadButton>(i), actionActive(static_cast<Action>(i), tMs));
    const auto full = static_cast<std::uint8_t>(actionActive(Action::Soft, tMs) ? std::lround(255 * softTrigger_) : 255);
    pad.l2 = actionActive(Action::L2, tMs) ? full : 0;
    pad.r2 = actionActive(Action::R2, tMs) ? full : 0;

    auto axis = [&](Action negative, Action positive) {
        const bool n = actionActive(negative, tMs), p = actionActive(positive, tMs);
        if (!n && !p) return 0.0f;
        if (n != p) return p ? 1.0f : -1.0f;
        return lastPress(positive, tMs) > lastPress(negative, tMs) ? 1.0f : -1.0f;
    };
    float lx = axis(Action::MoveLeft, Action::MoveRight);
    float ly = axis(Action::MoveUp, Action::MoveDown);
    const float len = std::sqrt(lx * lx + ly * ly);
    if (len > 1.0f) {
        lx /= len;
        ly /= len;
    }
    if (actionActive(Action::Walk, tMs)) {
        lx *= walkScale_;
        ly *= walkScale_;
    }
    pad.left = {lx, ly};
    pad.right = right;
    return pad;
}
