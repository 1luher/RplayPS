// SPDX-License-Identifier: AGPL-3.0-only
#include "core/profile.h"

#include <algorithm>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {

void warn(std::vector<std::string>* w, std::string text) {
    if (w) w->push_back(std::move(text));
}

template <typename T>
void readNumber(const json& obj, const char* key, T& out, std::vector<std::string>* w) {
    if (!obj.contains(key)) return;
    const json& v = obj[key];
    if (!v.is_number()) {
        warn(w, std::string("Профиль: «") + key + "» должно быть числом, значение пропущено.");
        return;
    }
    out = v.get<T>();
}

void readBool(const json& obj, const char* key, bool& out, std::vector<std::string>* w) {
    if (!obj.contains(key)) return;
    const json& v = obj[key];
    if (!v.is_boolean()) {
        warn(w, std::string("Профиль: «") + key + "» должно быть true или false, значение пропущено.");
        return;
    }
    out = v.get<bool>();
}

}  // namespace

Profile defaultGtaProfile() {
    Profile p;
    auto bind = [&](Action a, InputCode first, InputCode second = code::None) {
        p.bindings.slots[static_cast<std::size_t>(a)] = {first, second};
    };
    bind(Action::MoveUp, 'W');
    bind(Action::MoveLeft, 'A');
    bind(Action::MoveDown, 'S');
    bind(Action::MoveRight, 'D');
    bind(Action::Walk, vk::LAlt);
    bind(Action::Cross, vk::LShift);       // sprint
    bind(Action::Square, vk::Space);       // jump
    bind(Action::Triangle, 'F', code::WheelUp);  // enter vehicle
    bind(Action::Circle, 'R');             // reload
    bind(Action::R2, code::MouseLeft);     // shoot
    bind(Action::L2, code::MouseRight);    // aim
    bind(Action::L1, vk::Tab);             // weapon wheel
    bind(Action::R1, 'Q');                 // cover
    bind(Action::L3, vk::LCtrl);           // crouch
    bind(Action::R3, 'C');                 // look behind
    bind(Action::DpadUp, vk::Up);          // phone
    bind(Action::DpadDown, vk::Down);
    bind(Action::DpadLeft, vk::Left);
    bind(Action::DpadRight, vk::Right);
    bind(Action::Touchpad, 'M');           // map
    bind(Action::Options, vk::Esc);        // pause
    return p;
}

std::string profileToJson(const Profile& p) {
    json bindings = json::object();
    for (std::size_t i = 0; i < kActionCount; ++i) {
        json keys = json::array();
        for (InputCode c : p.bindings.slots[i])
            if (c != code::None) keys.push_back(inputCodeName(c));
        if (!keys.empty()) bindings[std::string(actionName(static_cast<Action>(i)))] = keys;
    }
    const MouseSettings& m = p.mouse;
    json j = {
        {"name", p.name},
        {"walk_scale", p.walkScale},
        {"mouse",
         {{"sensitivity", m.sensitivity},
          {"y_ratio", m.yRatio},
          {"invert_y", m.invertY},
          {"exponent", m.exponent},
          {"anti_deadzone", m.antiDeadzone},
          {"outer_limit", m.outerLimit},
          {"window_ms", m.windowMs},
          {"recenter_ms", m.recenterMs}}},
        {"bindings", bindings},
    };
    return j.dump(2);
}

std::optional<Profile> profileFromJson(std::string_view text, std::vector<std::string>* w) {
    const json j = json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) {
        warn(w, "Профиль: файл повреждён (это не JSON-объект).");
        return std::nullopt;
    }

    Profile p;
    if (j.contains("name") && j["name"].is_string()) p.name = j["name"].get<std::string>();
    readNumber(j, "walk_scale", p.walkScale, w);
    p.walkScale = std::clamp(p.walkScale, 0.1f, 1.0f);

    if (j.contains("mouse") && j["mouse"].is_object()) {
        const json& m = j["mouse"];
        readNumber(m, "sensitivity", p.mouse.sensitivity, w);
        readNumber(m, "y_ratio", p.mouse.yRatio, w);
        readBool(m, "invert_y", p.mouse.invertY, w);
        readNumber(m, "exponent", p.mouse.exponent, w);
        readNumber(m, "anti_deadzone", p.mouse.antiDeadzone, w);
        readNumber(m, "outer_limit", p.mouse.outerLimit, w);
        readNumber(m, "window_ms", p.mouse.windowMs, w);
        readNumber(m, "recenter_ms", p.mouse.recenterMs, w);
    }
    p.mouse.clamp();

    if (j.contains("bindings") && j["bindings"].is_object()) {
        for (const auto& [actionKey, keys] : j["bindings"].items()) {
            const auto action = parseAction(actionKey);
            if (!action) {
                warn(w, "Профиль: неизвестное действие «" + actionKey + "», пропущено.");
                continue;
            }
            if (!keys.is_array()) {
                warn(w, "Профиль: у «" + actionKey + "» должен быть список клавиш, пропущено.");
                continue;
            }
            auto& slots = p.bindings.slots[static_cast<std::size_t>(*action)];
            std::size_t used = 0;
            for (const json& k : keys) {
                const std::string name = k.is_string() ? k.get<std::string>() : k.dump();
                const auto c = k.is_string() ? parseInputCode(name) : std::nullopt;
                if (!c) {
                    warn(w, "Профиль: неизвестная клавиша «" + name + "» у «" + actionKey + "», пропущена.");
                    continue;
                }
                if (used == slots.size()) {
                    warn(w, "Профиль: у «" + actionKey + "» больше двух клавиш, лишние пропущены.");
                    break;
                }
                slots[used++] = *c;
            }
        }
    }
    return p;
}
