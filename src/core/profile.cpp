// SPDX-License-Identifier: AGPL-3.0-only
#include "core/profile.h"

#include <algorithm>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace {

constexpr std::string_view kClassNames[kControlClassCount] = {"foot", "ground", "water", "heli", "plane"};
constexpr std::string_view kClassTitles[kControlClassCount] = {"Пешком", "Наземный", "Вода", "Вертолёт", "Самолёт"};

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
    const double d = std::clamp(v.get<double>(), -1e6, 1e6);  // no UB on huge values
    out = static_cast<T>(d);
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

using BindFn = void (*)(Layout&);
void bind(Layout& l, Action a, InputCode first, InputCode second = code::None) {
    l.bindings.slots[static_cast<std::size_t>(a)] = {first, second};
}

Layout footLayout() {
    Layout l;
    bind(l, Action::MoveUp, 'W');
    bind(l, Action::MoveLeft, 'A');
    bind(l, Action::MoveDown, 'S');
    bind(l, Action::MoveRight, 'D');
    bind(l, Action::Walk, vk::LAlt);
    bind(l, Action::L3, vk::LShift, 'X');      // run (again: sprint); X = Focus (L3+R3)
    bind(l, Action::R3, code::MouseMiddle, 'X');  // look back; Focus with X
    bind(l, Action::Cross, vk::Space);         // jump / climb / dodge
    bind(l, Action::Circle, vk::LCtrl);        // crouch (hold: rest)
    bind(l, Action::Square, 'R');              // melee
    bind(l, Action::Triangle, 'F');            // interact / enter vehicle / quick swap
    bind(l, Action::R1, 'Q');                  // cover (hold: quick throw)
    bind(l, Action::L1, vk::Tab);              // holster (hold: loadout wheel)
    bind(l, Action::R2, code::MouseLeft);      // shoot
    bind(l, Action::L2, code::MouseRight);     // aim / talk
    bind(l, Action::DpadUp, vk::Up);           // phone (hold: WAiNK)
    bind(l, Action::DpadDown, vk::Down);       // HUD (hold: quick navigation)
    bind(l, Action::DpadLeft, vk::Left);       // swap shoulder / radio
    bind(l, Action::DpadRight, vk::Right);     // detonate / partner menu
    bind(l, Action::Touchpad, 'M');            // map
    bind(l, Action::Options, vk::Esc);         // pause
    return l;
}

Layout groundLayout() {  // cars, bikes, trucks, motorcycles; boats use the same
    Layout l;
    l.steerRampMs = 140;                       // A/D steer progressively
    bind(l, Action::Soft, vk::LAlt);           // gentle throttle while held
    bind(l, Action::R2, 'W');                  // throttle
    bind(l, Action::L2, 'S');                  // brake / reverse
    bind(l, Action::MoveLeft, 'A');            // steering
    bind(l, Action::MoveRight, 'D');
    bind(l, Action::MoveUp, 'Q');              // motorcycle: lean forward
    bind(l, Action::MoveDown, 'E');            // motorcycle: lean back
    bind(l, Action::Cross, vk::Space);         // handbrake
    bind(l, Action::Triangle, 'F');            // exit (hold: stop & exit, double: bail)
    bind(l, Action::L3, vk::LShift, 'H');      // horn / nitrous / siren
    bind(l, Action::Circle, 'L', vk::LCtrl);   // lights (tap), duck (hold) — confirmed in GTA 6
    bind(l, Action::R1, code::MouseLeft);      // drive-by shoot
    bind(l, Action::L1, code::MouseRight, vk::Tab);  // draw weapon (hold: vehicle weapons)
    bind(l, Action::R3, code::MouseMiddle, 'C');     // look back (click: shout)
    bind(l, Action::Square, 'R');              // melee
    bind(l, Action::DpadUp, vk::Up);           // phone
    bind(l, Action::DpadDown, vk::Down);       // vehicle info
    bind(l, Action::DpadLeft, vk::Left);       // indicator (hold: vehicle menu)
    bind(l, Action::DpadRight, vk::Right);     // indicator
    bind(l, Action::Touchpad, 'M');
    bind(l, Action::Options, vk::Esc);
    return l;
}

Layout airLayout() {  // GTA 6 has one "air vehicles" scheme; mouse = pitch/roll
    Layout l;
    l.mouseTarget = MouseTarget::LeftStick;
    bind(l, Action::R2, 'W');                  // climb / throttle up
    bind(l, Action::L2, 'S');                  // descend / throttle down
    bind(l, Action::Square, 'A');              // yaw left
    bind(l, Action::Circle, 'D');              // yaw right
    bind(l, Action::R1, code::MouseLeft);      // fire
    bind(l, Action::L1, code::MouseRight, vk::Tab);  // select weapon
    bind(l, Action::Triangle, 'F');            // exit (plane on water: take off)
    bind(l, Action::L3, 'G');                  // landing gear
    bind(l, Action::R3, code::MouseMiddle, 'C');
    bind(l, Action::DpadUp, vk::Up);
    bind(l, Action::DpadDown, vk::Down);
    bind(l, Action::DpadLeft, vk::Left);
    bind(l, Action::DpadRight, vk::Right);
    bind(l, Action::Touchpad, 'M');
    bind(l, Action::Options, vk::Esc);
    return l;
}

json layoutToJson(const Layout& l) {
    json bindings = json::object();
    for (std::size_t i = 0; i < kActionCount; ++i) {
        json keys = json::array();
        for (InputCode c : l.bindings.slots[i])
            if (c != code::None) keys.push_back(inputCodeName(c));
        if (!keys.empty()) bindings[std::string(actionName(static_cast<Action>(i)))] = keys;
    }
    const MouseSettings& m = l.mouse;
    return {
        {"walk_scale", l.walkScale},
        {"steer_ramp_ms", l.steerRampMs},
        {"soft_trigger", l.softTrigger},
        {"mouse_target", l.mouseTarget == MouseTarget::LeftStick ? "left" : "right"},
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
}

// Fields present in `j` override `l`; a present "bindings" object replaces all bindings.
void readLayout(const json& j, Layout& l, std::vector<std::string>* w) {
    readNumber(j, "walk_scale", l.walkScale, w);
    l.walkScale = std::clamp(l.walkScale, 0.1f, 1.0f);
    readNumber(j, "steer_ramp_ms", l.steerRampMs, w);
    l.steerRampMs = std::clamp(l.steerRampMs, 0, 500);
    readNumber(j, "soft_trigger", l.softTrigger, w);
    l.softTrigger = std::clamp(l.softTrigger, 0.1f, 1.0f);
    if (j.contains("mouse_target")) {
        const json& t = j["mouse_target"];
        if (t == "left")
            l.mouseTarget = MouseTarget::LeftStick;
        else if (t == "right")
            l.mouseTarget = MouseTarget::RightStick;
        else
            warn(w, "Профиль: «mouse_target» должно быть \"left\" или \"right\", значение пропущено.");
    }
    if (j.contains("mouse") && j["mouse"].is_object()) {
        const json& m = j["mouse"];
        readNumber(m, "sensitivity", l.mouse.sensitivity, w);
        readNumber(m, "y_ratio", l.mouse.yRatio, w);
        readBool(m, "invert_y", l.mouse.invertY, w);
        readNumber(m, "exponent", l.mouse.exponent, w);
        readNumber(m, "anti_deadzone", l.mouse.antiDeadzone, w);
        readNumber(m, "outer_limit", l.mouse.outerLimit, w);
        readNumber(m, "window_ms", l.mouse.windowMs, w);
        readNumber(m, "recenter_ms", l.mouse.recenterMs, w);
    }
    l.mouse.clamp();

    if (!j.contains("bindings")) return;
    if (!j["bindings"].is_object()) {
        warn(w, "Профиль: «bindings» должно быть объектом, пропущено.");
        return;
    }
    l.bindings = Bindings{};
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
        auto& slots = l.bindings.slots[static_cast<std::size_t>(*action)];
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

}  // namespace

std::string_view controlClassName(ControlClass c) {
    const auto i = static_cast<std::size_t>(c);
    return i < kControlClassCount ? kClassNames[i] : std::string_view{};
}

std::string_view controlClassTitleRu(ControlClass c) {
    const auto i = static_cast<std::size_t>(c);
    return i < kControlClassCount ? kClassTitles[i] : std::string_view{};
}

std::optional<ControlClass> parseControlClass(std::string_view name) {
    for (std::size_t i = 0; i < kControlClassCount; ++i)
        if (kClassNames[i] == name) return static_cast<ControlClass>(i);
    return std::nullopt;
}

Profile defaultGtaProfile() {
    Profile p;
    p.layout(ControlClass::Foot) = footLayout();
    p.layout(ControlClass::Ground) = groundLayout();
    p.layout(ControlClass::Water) = groundLayout();
    p.layout(ControlClass::Heli) = airLayout();
    p.layout(ControlClass::Plane) = airLayout();
    return p;
}

std::string profileToJson(const Profile& p) {
    json layouts = json::object();
    for (std::size_t i = 0; i < kControlClassCount; ++i)
        layouts[std::string(kClassNames[i])] = layoutToJson(p.layouts[i]);
    return json{{"name", p.name}, {"layouts", layouts}}.dump(2);
}

std::optional<Profile> profileFromJson(std::string_view text, std::vector<std::string>* w) {
    const json j = json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) {
        warn(w, "Профиль: файл повреждён (это не JSON-объект).");
        return std::nullopt;
    }
    Profile p = defaultGtaProfile();
    if (j.contains("name") && j["name"].is_string()) p.name = j["name"].get<std::string>();

    if (j.contains("layouts") && j["layouts"].is_object()) {
        for (const auto& [key, value] : j["layouts"].items()) {
            const auto c = parseControlClass(key);
            if (!c) {
                warn(w, "Профиль: неизвестный класс «" + key + "», пропущен.");
                continue;
            }
            if (!value.is_object()) {
                warn(w, "Профиль: класс «" + key + "» должен быть объектом, пропущен.");
                continue;
            }
            readLayout(value, p.layout(*c), w);
        }
    } else if (j.contains("bindings") || j.contains("mouse") || j.contains("walk_scale")) {
        Layout legacy;  // old single-layout file: it describes "foot" completely
        readLayout(j, legacy, w);
        p.layout(ControlClass::Foot) = legacy;
    }
    return p;
}

PadState composePad(const PadBuilder& b, MouseTarget target, Stick mouse, double tMs) {
    if (target == MouseTarget::RightStick) return b.build(tMs, mouse);
    PadState s = b.build(tMs, {});
    if (mouse != Stick{}) s.left = mouse;  // the mouse wins over keys bound to the left stick
    return s;
}

ClassPads::ClassPads(const Profile& p) : profile_(p) {
    builders_.reserve(kControlClassCount);
    for (const Layout& l : profile_.layouts) builders_.emplace_back(l.bindings, l.walkScale, l.softTrigger);
}

void ClassPads::setInput(InputCode c, bool down, double tMs) {
    for (auto& b : builders_) b.setInput(c, down, tMs);
}

void ClassPads::releaseAll() {
    for (auto& b : builders_) b.releaseAll();
}

PadState ClassPads::build(double tMs, Stick mouse) const {
    const auto i = static_cast<std::size_t>(active_);
    return composePad(builders_[i], profile_.layouts[i].mouseTarget, mouse, tMs);
}
