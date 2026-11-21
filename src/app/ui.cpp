// SPDX-License-Identifier: AGPL-3.0-only
#include "app/ui.h"

#include <imgui.h>
#include <shellapi.h>
#include <shlobj.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "core/account_id.h"

namespace {

struct StateLook {
    const char* text;
    ImU32 color;
};

StateLook stateLook(LinkState s) {
    switch (s) {
        case LinkState::Idle: return {"Не подключено", col::Muted};
        case LinkState::Searching: return {"Поиск PS5", col::Warn};
        case LinkState::Waking: return {"Пробуждение", col::Warn};
        case LinkState::Connecting: return {"Подключение", col::Warn};
        case LinkState::Connected: return {"Подключено", col::Good};
        case LinkState::Lost: return {"Связь потеряна", col::Bad};
    }
    return {"", col::Muted};
}

template <typename T>
bool ready(const std::future<T>& f) {
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

ImU32 withAlpha(ImU32 c, int a) { return (c & 0x00FFFFFF) | (static_cast<ImU32>(a) << 24); }

// Rounded "pill" with a dot: e.g. ● Подключено
void pill(const char* text, ImU32 color, ImFont* font) {
    ImGui::PushFont(font);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const float h = ts.y + 12, dot = 8, padX = 14;
    const ImVec2 size(padX + dot + 8 + ts.x + padX, h);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), withAlpha(color, 34), h * 0.5f);
    dl->AddRect(p, ImVec2(p.x + size.x, p.y + size.y), withAlpha(color, 90), h * 0.5f);
    dl->AddCircleFilled(ImVec2(p.x + padX + dot * 0.5f, p.y + h * 0.5f), dot * 0.5f, color);
    dl->AddText(ImVec2(p.x + padX + dot + 8, p.y + 6), color, text);
    ImGui::Dummy(size);
    ImGui::PopFont();
}

// Keyboard key cap, e.g. [F8]
void keycap(const char* key, ImFont* font) {
    ImGui::PushFont(font);
    const ImVec2 ts = ImGui::CalcTextSize(key);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 size(ts.x + 16, ts.y + 8);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, p.y + 2), ImVec2(p.x + size.x, p.y + size.y + 2), col::Line, 6);
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col::CardHi, 6);
    dl->AddText(ImVec2(p.x + 8, p.y + 4), col::Text, key);
    ImGui::Dummy(ImVec2(size.x, size.y + 2));
    ImGui::PopFont();
}

bool beginCard(const char* id, const char* iconGlyph, const char* title, const Fonts& f, float height = 0) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 16));
    const bool open = ImGui::BeginChild(id, ImVec2(0, height),
                                        ImGuiChildFlags_Borders | (height == 0 ? ImGuiChildFlags_AutoResizeY : 0));
    ImGui::PopStyleVar();
    ImGui::PushFont(f.semibold);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Accent));
    ImGui::TextUnformatted(iconGlyph);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 2));
    return open;
}

void endCard() { ImGui::EndChild(); }

bool primaryButton(const char* label, float width = 0) {
    ImGui::PushStyleColor(ImGuiCol_Button, toVec4(col::Accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(IM_COL32(255, 116, 158, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(IM_COL32(226, 66, 114, 255)));
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(IM_COL32(255, 255, 255, 255)));
    const bool pressed = ImGui::Button(label, ImVec2(width, 0));
    ImGui::PopStyleColor(4);
    return pressed;
}

bool ghostButton(const char* label, float width = 0) { return ImGui::Button(label, ImVec2(width, 0)); }

void mutedText(const char* text, const Fonts& f) {
    ImGui::PushFont(f.tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void noteText(const std::string& text, ImU32 color, const Fonts& f) {
    if (text.empty()) return;
    ImGui::PushFont(f.tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(color));
    ImGui::TextWrapped("%s", text.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

constexpr float kNumberFieldWidth = 84;

// One compact row: muted label on the left (hint on hover), slider, then a field to type the number.
void rowLabel(const char* label, const char* hint) {
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    if (hint) ImGui::SetItemTooltip("%s", hint);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.36f + ImGui::GetCursorStartPos().x);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - kNumberFieldWidth - ImGui::GetStyle().ItemSpacing.x);
}

bool sliderFloat(const char* label, const char* hint, float* v, float lo, float hi, const char* fmt,
                 ImGuiSliderFlags flags = 0) {
    rowLabel(label, hint);
    const std::string id = std::string("##") + label;
    bool changed = ImGui::SliderFloat(id.c_str(), v, lo, hi, fmt, flags | ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kNumberFieldWidth);
    if (ImGui::InputFloat((id + "_n").c_str(), v, 0, 0, "%.2f")) {
        *v = std::clamp(*v, lo, hi);
        changed = true;
    }
    return changed;
}

bool sliderInt(const char* label, const char* hint, int* v, int lo, int hi, const char* fmt) {
    rowLabel(label, hint);
    const std::string id = std::string("##") + label;
    bool changed = ImGui::SliderInt(id.c_str(), v, lo, hi, fmt, ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(kNumberFieldWidth);
    if (ImGui::InputInt((id + "_n").c_str(), v, 0, 0)) {
        *v = std::clamp(*v, lo, hi);
        changed = true;
    }
    return changed;
}

// "прыжок" -> "Прыжок" (UTF-8 Cyrillic and Latin).
std::string capitalized(const char* s) {
    std::string out(s);
    if (out.size() >= 2) {
        auto* b = reinterpret_cast<unsigned char*>(out.data());
        if (b[0] == 0xD0 && b[1] >= 0xB0 && b[1] <= 0xBF) {  // а..п
            b[1] -= 0x20;
        } else if (b[0] == 0xD1 && b[1] >= 0x80 && b[1] <= 0x8F) {  // р..я
            b[0] = 0xD0;
            b[1] += 0x20;
        } else if (b[0] == 0xD1 && b[1] == 0x91) {  // ё
            b[0] = 0xD0;
            b[1] = 0x81;
        }
    }
    if (!out.empty() && out[0] >= 'a' && out[0] <= 'z') out[0] = static_cast<char>(out[0] - 'a' + 'A');
    return out;
}

bool inputField(const char* label, const char* hint, char* buf, size_t size, const Fonts& f,
                ImGuiInputTextFlags flags = 0) {
    ImGui::PushFont(f.tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SetNextItemWidth(-FLT_MIN);
    const std::string id = std::string("##") + label;
    return ImGui::InputTextWithHint(id.c_str(), hint, buf, size, flags);
}

void stickView(const char* label, Stick s, float radius, const Fonts& f) {
    ImGui::BeginGroup();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 c(p.x + radius, p.y + radius);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(c, radius, IM_COL32(13, 13, 17, 255), 64);
    dl->AddCircle(c, radius, col::Line, 64, 1.5f);
    dl->AddCircle(c, radius * 0.5f, withAlpha(col::Line, 140), 48, 1.0f);
    dl->AddLine(ImVec2(c.x - radius + 8, c.y), ImVec2(c.x + radius - 8, c.y), withAlpha(col::Line, 160));
    dl->AddLine(ImVec2(c.x, c.y - radius + 8), ImVec2(c.x, c.y + radius - 8), withAlpha(col::Line, 160));
    const ImVec2 dot(c.x + s.x * (radius - 10), c.y + s.y * (radius - 10));
    const bool active = std::hypot(s.x, s.y) > 0.01f;
    if (active) {
        dl->AddLine(c, dot, withAlpha(col::Accent, 120), 2.0f);
        for (int i = 4; i >= 1; --i) dl->AddCircleFilled(dot, 7.0f + i * 3.0f, withAlpha(col::Accent, 14));
    }
    dl->AddCircleFilled(dot, 8.0f, active ? col::Accent : col::Muted, 24);
    dl->AddCircleFilled(dot, 3.0f, IM_COL32(255, 255, 255, 220), 12);
    ImGui::Dummy(ImVec2(radius * 2, radius * 2 + 6));
    ImGui::PushFont(f.tiny);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s   %+.2f  %+.2f", label, s.x, s.y);
    const float w = ImGui::CalcTextSize(buf).x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, radius - w * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted(buf);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::EndGroup();
}

void statTile(const char* label, const char* value, const char* unit, float width, const Fonts& f) {
    ImGui::BeginGroup();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = 68;
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + width, p.y + h), IM_COL32(13, 13, 17, 255), 10);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 12, p.y + 8));
    ImGui::PushFont(f.tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(p.x + 12, p.y + 30));
    ImGui::PushFont(f.big);
    ImGui::TextUnformatted(value);
    ImGui::PopFont();
    ImGui::SameLine(0, 4);
    ImGui::PushFont(f.tiny);
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, p.y + 40));
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted(unit);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(width, h));
    ImGui::EndGroup();
}

void triggerBar(const char* label, std::uint8_t value, const Fonts& f) {
    ImGui::PushFont(f.tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine(36);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x, h = 8;
    const float y = p.y + 6;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), IM_COL32(13, 13, 17, 255), h);
    if (value) dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w * value / 255.0f, y + h), col::Accent, h);
    ImGui::Dummy(ImVec2(w, 20));
}

// ------------------------------------------------------------------ sections

// Opens the controller → keys hint (drawn by drawHint) on the mode that is active now.
void openHint(App& app) {
    app.hintRequested = true;
    app.hintClass = app.input->currentClass();
    app.hintSelectTab = true;
}

void drawHeader(App& app) {
    const Fonts& f = app.fonts;
    LinkState state;
    {
        std::lock_guard lock(app.statusMutex);
        state = app.linkState;
    }
    ImGui::PushFont(f.title);
    ImGui::TextUnformatted("RPlay K&M");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    mutedText("клавиатура и мышь для GTA 6 на PS5", f);

    // Right side: the hint button, then the link state pill.
    const StateLook look = app.consoleOnly ? StateLook{"Без консоли", col::Muted} : stateLook(state);
    ImGui::PushFont(f.semibold);
    const float pillW = ImGui::CalcTextSize(look.text).x + 14 + 8 + 8 + 14;
    ImGui::PopFont();
    const std::string hint = std::string(icon::Help) + "  Подсказка";
    const float hintW = ImGui::CalcTextSize(hint.c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - pillW - hintW - 12);
    ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 2);
    if (ghostButton(hint.c_str())) openHint(app);
    ImGui::SetItemTooltip("Кнопки геймпада из подсказок GTA 6 и ваши клавиши");
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - pillW);
    ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y + 4);
    pill(look.text, look.color, f.semibold);
    ImGui::Dummy(ImVec2(0, 4));
}

void drawCaptureBanner(App& app) {
    const Fonts& f = app.fonts;
    const bool captured = app.input->captured();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x, h = 62;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 tone = captured ? col::Good : col::Muted;  // calm when idle, green when live
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), captured ? withAlpha(col::Good, 30) : col::Card, 14);
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), captured ? withAlpha(col::Good, 140) : col::Line, 14, 0, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(p.x + 18, p.y + 14));
    ImGui::PushFont(f.title);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(tone));
    ImGui::TextUnformatted(icon::Keyboard);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine(0, 14);
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x, p.y + 10));
    ImGui::BeginGroup();
    ImGui::PushFont(f.semibold);
    ImGui::TextUnformatted(captured ? "Захват включён — управление уходит на PS5" : "Захват выключен");
    ImGui::SameLine(0, 14);
    {
        const std::string mode = "Режим: " + std::string(controlClassTitleRu(app.input->currentClass()));
        ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Accent));
        ImGui::TextUnformatted(mode.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopFont();
    mutedText(captured ? "Отпустить: F8. Если окно потеряет фокус, всё отпустится само."
                       : "Нажмите F8, затем переключите монитор на вход PS5.",
              f);
    ImGui::EndGroup();

    ImGui::PushFont(f.semibold);
    const float capW = ImGui::CalcTextSize("F8").x + 16;
    ImGui::PopFont();
    ImGui::SetCursorScreenPos(ImVec2(p.x + w - capW - 20, p.y + 16));
    keycap("F8", f.semibold);
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + h + 12));
    ImGui::Dummy(ImVec2(0, 0));
}

void drawPs5(App& app) {
    const Fonts& f = app.fonts;
    LinkState state;
    std::string message;
    {
        std::lock_guard lock(app.statusMutex);
        state = app.linkState;
        message = app.linkMessage;
    }
    beginCard("##ps5", icon::Game, "PS5", f);
    if (app.consoleOnly) {
        mutedText("Режим без консоли: на PS5 ничего не отправляется. Удобно настраивать мышь.", f);
        endCard();
        return;
    }

    if (app.pairing) {
        ImGui::PushFont(f.semibold);
        ImGui::TextUnformatted(app.pairing->name.empty() ? "PS5" : app.pairing->name.c_str());
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        mutedText(app.pairing->host.c_str(), f);
        noteText(message, state == LinkState::Lost ? col::Bad : col::Muted, f);
        ImGui::Dummy(ImVec2(0, 2));
        const bool busy = state != LinkState::Idle && state != LinkState::Lost;
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (!busy) {
            if (primaryButton("Подключить", half)) app.link->connect(*app.pairing);
        } else if (ghostButton("Отключить", half)) {
            app.link->disconnect();
        }
        ImGui::SameLine();
        if (ghostButton("Забыть PS5", half)) {
            app.link->disconnect();
            std::error_code ec;
            std::filesystem::remove(app.pairingPath, ec);
            app.pairing.reset();
        }
        endCard();
        return;
    }

    // Not paired yet.
    mutedText("На PS5: Настройки → Система → Дистанционное воспроизведение → Привязать устройство. "
              "Введите адрес, account ID и PIN с экрана.",
              f);
    const bool pairingBusy = app.pairing_job.valid();
    const bool findBusy = app.discover_job.valid();
    ImGui::Dummy(ImVec2(0, 2));
    {
        const std::string findLabel = std::string(icon::Search) + (findBusy ? "  Ищу…" : "  Найти PS5 в сети");
        ImGui::BeginDisabled(findBusy);
        if (ghostButton(findLabel.c_str(), -FLT_MIN)) {
            Ps5Link* link = app.link;
            app.discover_job =
                std::async(std::launch::async, [link] { return link->discover(std::chrono::milliseconds(2500)); });
        }
        ImGui::EndDisabled();
    }
    if (ready(app.discover_job)) {
        app.found = app.discover_job.get();
        if (app.found.empty()) app.pairError = "PS5 не найдена. Она включена и в той же сети? VPN выключен?";
    }
    for (const auto& c : app.found) {
        const std::string label = c.name + "   " + c.host + (c.ready ? "" : "   · спит");
        if (ImGui::Selectable(label.c_str(), false, 0, ImVec2(0, 26))) strncpy_s(app.hostField, c.host.c_str(), _TRUNCATE);
    }
    inputField("Адрес PS5", "192.168.1.50", app.hostField, sizeof(app.hostField), f);
    inputField("Account ID", "число или 12 символов base64", app.accountField, sizeof(app.accountField), f);
    inputField("PIN", "8 цифр с экрана PS5", app.pinField, sizeof(app.pinField), f, ImGuiInputTextFlags_CharsDecimal);
    ImGui::Dummy(ImVec2(0, 2));
    ImGui::BeginDisabled(pairingBusy);
    const std::string pairLabel = std::string(icon::Link) + (pairingBusy ? "  Привязываю… до 30 с" : "  Привязать");
    if (primaryButton(pairLabel.c_str(), -FLT_MIN)) {
        std::string error;
        const auto account = parseAccountId(app.accountField, &error);
        const auto pin = parsePin(app.pinField);
        std::string host = app.hostField;
        host.erase(0, host.find_first_not_of(" \t"));
        host.erase(host.find_last_not_of(" \t") + 1);
        if (!account) {
            app.pairError = error;
        } else if (!pin) {
            app.pairError = "PIN — это 8 цифр с экрана PS5.";
        } else {
            app.pairError.clear();
            Ps5Link* link = app.link;
            const AccountId acc = *account;
            const std::uint32_t pinValue = *pin;
            app.pairing_job = std::async(std::launch::async, [link, host, acc, pinValue] {
                std::string e;
                auto p = link->pair(host, acc, pinValue, &e);
                return std::make_pair(std::move(p), std::move(e));
            });
        }
    }
    ImGui::EndDisabled();
    if (ready(app.pairing_job)) {
        auto [p, error] = app.pairing_job.get();
        if (!p) app.pairError = error;
        if (p) {
            if (!savePairing(*p, app.pairingPath)) app.pairError = "Привязка прошла, но сохранить её не удалось.";
            app.pairing = *p;
            app.pinField[0] = '\0';
            app.link->connect(*app.pairing);
        }
    }
    noteText(app.pairError, col::Bad, f);
    endCard();
}

void drawMouse(App& app) {
    const Fonts& f = app.fonts;
    bool changed = false;
    beginCard("##mouse", icon::Mouse, "Мышь", f);
    // One tab per class: each class has its own mouse settings.
    if (ImGui::BeginTabBar("##classes")) {
        for (size_t i = 0; i < kControlClassCount; ++i) {
            const auto c = static_cast<ControlClass>(i);
            const std::string title(controlClassTitleRu(c));
            if (ImGui::BeginTabItem(title.c_str())) {
                app.editClass = c;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    Layout& layout = app.profile.layout(app.editClass);
    MouseSettings& m = layout.mouse;
    const bool vehicle = app.editClass == ControlClass::Ground || app.editClass == ControlClass::Water;
    mutedText(layout.mouseTarget == MouseTarget::LeftStick ? "Мышь здесь управляет наклонами (левый стик)."
                                                           : "Мышь здесь управляет камерой (правый стик).",
              f);
    ImGui::Dummy(ImVec2(0, 2));

    // What almost everyone needs.
    changed |= sliderFloat("Чувствительность", "Насколько поворачивает камера при том же движении мыши. "
                                               "Можно вписать число в поле справа.",
                           &m.sensitivity, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    changed |= sliderFloat("Вертикаль", "Чувствительность по вертикали относительно горизонтали", &m.yRatio, 0.25f,
                           3.0f, "× %.2f");
    if (vehicle) {
        changed |= sliderInt("Плавность руля", "За сколько миллисекунд A/D доводят руль до упора (0 — сразу)",
                             &layout.steerRampMs, 0, 500, "%d мс");
        changed |= sliderFloat("Мягкий газ (Alt)", "Сила газа и тормоза, пока зажат Alt", &layout.softTrigger, 0.1f,
                               1.0f, "%.2f");
    }
    changed |= ImGui::Checkbox("Инвертировать ось Y", &m.invertY);

    // Everything else: folded away.
    ImGui::Dummy(ImVec2(0, 4));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    if (ImGui::CollapsingHeader("Тонкая настройка")) {
        changed |= sliderFloat("Кривая", "1 — линейно; больше — точнее на медленных движениях", &m.exponent, 0.5f, 3.0f,
                               "%.2f");
        changed |= sliderFloat("Анти-мёртвая зона", "Сразу перепрыгивает мёртвую зону стика в игре", &m.antiDeadzone,
                               0.0f, 0.5f, "%.2f");
        changed |= sliderFloat("Макс. наклон", "Сильнее этого стик не отклоняется", &m.outerLimit, 0.5f, 1.0f, "%.2f");
        changed |= sliderInt("Окно скорости", "Меньше — быстрее отклик, больше — плавнее", &m.windowMs, 2, 32, "%d мс");
        changed |= sliderInt("Возврат в центр", "Через сколько стик встаёт в центр, когда мышь остановилась",
                             &m.recenterMs, 2, 100, "%d мс");
        if (app.editClass == ControlClass::Foot)
            changed |= sliderFloat("Шаг (Alt)", "Насколько наклоняется левый стик, пока зажат Alt", &layout.walkScale,
                                   0.1f, 1.0f, "%.2f");
    }
    ImGui::PopStyleColor();

    if (changed) {
        m.clamp();
        app.input->setProfile(app.profile);
        app.mouseDirty = true;
    }
    // Saved automatically once you let go of the slider / leave the field.
    if (app.mouseDirty && !ImGui::IsAnyItemActive()) {
        saveProfile(app);
        app.mouseDirty = false;
    }
    if (!app.saveNote.empty()) noteText(app.saveNote == "Сохранено" ? "Сохранено автоматически" : app.saveNote,
                                        app.saveNote == "Сохранено" ? col::Muted : col::Bad, f);
    for (const auto& w : app.profileWarnings) noteText(w, col::Warn, f);
    endCard();
}

void drawLive(App& app) {
    const Fonts& f = app.fonts;
    const InputStats s = app.input->stats();
    beginCard("##live", icon::Pulse, "Что уходит на PS5", f);
    const float avail = ImGui::GetContentRegionAvail().x;
    const float radius = std::min(86.0f, (avail - 40) * 0.25f);
    const float gap = avail - radius * 4;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gap / 3);
    stickView("Левый", s.lastPad.left, radius, f);
    ImGui::SameLine(0, gap / 3);
    stickView("Правый", s.lastPad.right, radius, f);
    ImGui::Dummy(ImVec2(0, 4));
    triggerBar("L2", s.lastPad.l2, f);
    triggerBar("R2", s.lastPad.r2, f);
    ImGui::Dummy(ImVec2(0, 4));
    const float tileW = (avail - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
    char a[32], b[32], c[32];
    std::snprintf(a, sizeof(a), "%.0f", s.mouseHz);
    std::snprintf(b, sizeof(b), "%.2f", s.tickJitterUs / 1000.0);
    int pressed = 0;
    for (unsigned bits = s.lastPad.buttons; bits; bits &= bits - 1) ++pressed;
    std::snprintf(c, sizeof(c), "%d", pressed);
    statTile("Мышь", a, "Гц", tileW, f);
    ImGui::SameLine();
    statTile("Опоздание тика", b, "мс", tileW, f);
    ImGui::SameLine();
    statTile("Кнопки нажаты", c, "", tileW, f);
    endCard();
}

void drawNetwork(App& app) {
    const Fonts& f = app.fonts;
    beginCard("##net", icon::Pulse, "Сеть до PS5", f);
    const NetQuality q = app.link->quality();
    const bool connected = app.link->state() == LinkState::Connected;
    if (!connected || q.grade == NetGrade::Unknown) {
        mutedText(connected ? "Измеряю… первые цифры появятся через пару секунд."
                            : "Появится после подключения к PS5: пинг, джиттер и потери, раз в 2 секунды.",
                  f);
        endCard();
        return;
    }
    struct Look {
        const char* text;
        ImU32 color;
        const char* advice;
    };
    const Look look = q.grade == NetGrade::Excellent ? Look{"Отлично", col::Good, "Сеть не мешает."}
                    : q.grade == NetGrade::Good      ? Look{"Хорошо", col::Good, "Задержка почти незаметна."}
                    : q.grade == NetGrade::Fair
                        ? Look{"Терпимо", col::Warn,
                               "Управление может временами «вязнуть». Wi-Fi 5 ГГц, роутер ближе или кабель помогут."}
                        : Look{"Плохо", col::Bad,
                               "Заметные рывки и задержки. Подключите PS5 кабелем или к Wi-Fi 5 ГГц рядом с роутером."};
    pill(look.text, look.color, f.semibold);
    ImGui::Dummy(ImVec2(0, 2));
    const float tileW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
    char a[32], b[32], c[32];
    std::snprintf(a, sizeof(a), "%.0f", q.pingMs);
    std::snprintf(b, sizeof(b), "%.0f", q.jitterMs);
    std::snprintf(c, sizeof(c), "%.1f", std::max(q.probeLossPct, q.frameLossPct));
    statTile("Пинг", a, "мс", tileW, f);
    ImGui::SameLine();
    statTile("Джиттер", b, "мс", tileW, f);
    ImGui::SameLine();
    statTile("Потери", c, "%", tileW, f);
    mutedText(look.advice, f);
    mutedText("Это задержка сети между ПК и PS5, без учёта игры и монитора.", f);
    endCard();
}

void drawSidebar(App& app) {
    const Fonts& f = app.fonts;
    struct Item {
        Page page;
        const char* iconGlyph;
        const char* title;
    };
    static const Item items[] = {
        {Page::Home, icon::Home, "Главная"},     {Page::Mouse, icon::Mouse, "Мышь"},
        {Page::Keys, icon::Keyboard, "Раскладка"}, {Page::Record, icon::Record, "Запись"},
        {Page::Help, icon::Help, "Справка"},
    };
    ImGui::BeginChild("##nav", ImVec2(200, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const Item& it : items) {
        const bool on = app.page == it.page;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, 46);
        ImGui::PushID(static_cast<int>(it.page));
        if (ImGui::InvisibleButton("##item", size)) app.page = it.page;
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (on || hover) dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), on ? col::CardHi : col::Card, 10);
        if (on) dl->AddRectFilled(ImVec2(p.x, p.y + 12), ImVec2(p.x + 3, p.y + size.y - 12), col::Accent, 2);
        ImGui::PushFont(on ? f.semibold : f.body);
        const float th = ImGui::GetTextLineHeight();
        dl->AddText(ImVec2(p.x + 16, p.y + (size.y - th) * 0.5f), on ? col::Accent : col::Muted, it.iconGlyph);
        dl->AddText(ImVec2(p.x + 46, p.y + (size.y - th) * 0.5f), on ? col::Text : col::Muted, it.title);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 2));
    }
    ImGui::EndChild();
}

void drawFirstSteps(App& app) {
    const Fonts& f = app.fonts;
    beginCard("##steps", icon::Help, "Как начать", f);
    static const char* steps[] = {
        "Привяжите PS5: адрес, account ID и PIN из «Настройки → Система → Дистанционное воспроизведение».",
        "Нажмите F8 — клавиатура и мышь начнут управлять PS5.",
        "Переключите монитор на вход PS5 и играйте. F8 ещё раз — вернуть их компьютеру.",
    };
    for (int i = 0; i < 3; ++i) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddCircleFilled(ImVec2(p.x + 12, p.y + 12), 12, col::AccentSoft);
        char n[2] = {static_cast<char>('1' + i), 0};
        ImGui::PushFont(f.semibold);
        dl->AddText(ImVec2(p.x + 8, p.y + 2), col::Accent, n);
        ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(p.x + 34, p.y + 2));
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(steps[i]);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 4));
    }
    mutedText("Настройки мыши, раскладка и запись — в меню слева.", f);
    endCard();
}

void drawKeys(App& app) {
    const Fonts& f = app.fonts;
    beginCard("##keys", icon::Keyboard, "Горячие клавиши", f);
    if (primaryButton((std::string(icon::Help) + "  Подсказка: кнопки геймпада → клавиши").c_str(), -FLT_MIN))
        openHint(app);
    ImGui::Dummy(ImVec2(0, 4));
    struct Row {
        const char* key;
        const char* what;
    };
    static const Row rows[] = {
        {"F8", "захват вкл / выкл"},
        {"WASD", "движение"},
        {"LAlt", "шаг"},
        {"ЛКМ / ПКМ", "R2 / L2"},
        {"Home", "кнопка PS (держать — забрать управление консолью)"},
        {"F1–F5", "режим: пешком, наземный, вода, вертолёт, самолёт (временно, до автоопределения)"},
    };
    for (const Row& r : rows) {
        keycap(r.key, f.tiny);
        ImGui::SameLine(0, 12);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
        mutedText(r.what, f);
    }
    endCard();
}

// ------------------------------------------------------------------ key binding editor

const char* padButtonRu(Action a) {
    switch (a) {
        case Action::Cross: return "Крест";
        case Action::Circle: return "Круг";
        case Action::Square: return "Квадрат";
        case Action::Triangle: return "Треугольник";
        case Action::DpadUp: return "D-pad ↑";
        case Action::DpadDown: return "D-pad ↓";
        case Action::DpadLeft: return "D-pad ←";
        case Action::DpadRight: return "D-pad →";
        case Action::L1: return "L1";
        case Action::R1: return "R1";
        case Action::L3: return "L3";
        case Action::R3: return "R3";
        case Action::Options: return "Options";
        case Action::Create: return "Create";
        case Action::Touchpad: return "Тачпад";
        case Action::PS: return "PS";
        case Action::L2: return "L2";
        case Action::R2: return "R2";
        case Action::MoveUp: return "Л. стик ↑";
        case Action::MoveDown: return "Л. стик ↓";
        case Action::MoveLeft: return "Л. стик ←";
        case Action::MoveRight: return "Л. стик →";
        case Action::Walk: return "модификатор";
        case Action::Soft: return "модификатор";
        case Action::Count: break;
    }
    return "";
}

// What the button does in GTA 6 for this class: the game's Settings → Control scheme screens
// (Default scheme). "Foot" also covers aiming, melee, swimming and the parachute; "ground" and
// "water" are one game screen and also cover the bicycle; heli and plane share the air screen.
const char* actionMeaningRu(ControlClass c, Action a) {
    const bool air = c == ControlClass::Heli || c == ControlClass::Plane;
    const bool vehicle = c == ControlClass::Ground || c == ControlClass::Water;
    switch (a) {
        case Action::MoveUp: return air ? "тангаж вперёд (обычно мышь)" : vehicle ? "наклон вперёд (мото, велосипед)" : "вперёд";
        case Action::MoveDown: return air ? "тангаж назад (обычно мышь)" : vehicle ? "наклон назад (мото, велосипед)" : "назад";
        case Action::MoveLeft: return air ? "крен влево (обычно мышь)" : vehicle ? "повернуть влево" : "влево";
        case Action::MoveRight: return air ? "крен вправо (обычно мышь)" : vehicle ? "повернуть вправо" : "вправо";
        case Action::Walk: return "шаг: стик наклоняется не до конца";
        case Action::Soft: return "мягкий газ / тормоз (L2, R2 не в пол)";
        case Action::R2:
            return air ? "прибавить газ" : vehicle ? "ускорение; велосипед — педали"
                                                   : "выстрелить; ближний бой — схватить; парашют — правая клеванта";
        case Action::L2:
            return air ? "сбросить газ" : vehicle ? "тормоз / задний ход"
                                                  : "прицеливание / взаимодействие; бой — захват цели; парашют — левая клеванта";
        case Action::Cross:
            return air ? "—" : vehicle ? "ручной тормоз; велосипед — ускорение"
                                       : "прыжок / лазание; бой — блок; вода — вынырнуть; парашют — раскрыть";
        case Action::Circle:
            return air ? "курс вправо" : vehicle ? "фары (держать — пригнуться); велосипед — прыжок"
                                                 : "присесть (держать — отдохнуть)";
        case Action::Square: return air ? "курс влево" : "ближний бой";
        case Action::Triangle:
            return c == ControlClass::Plane ? "покинуть транспорт; на воде — взлёт"
                   : air                    ? "покинуть транспорт"
                   : vehicle                ? "покинуть транспорт; велосипед — слезть"
                                            : "взаимодействовать; с прицелом — быстрая смена оружия; парашют — отцепить";
        case Action::R1: return vehicle || air ? "выстрелить" : "укрытие; с прицелом — быстрый бросок; вода — нырнуть";
        case Action::L1: return vehicle || air ? "кобура / снаряжение" : "снаряжение (держать — колесо оружия)";
        case Action::L3:
            return air ? "шасси" : vehicle ? "клаксон / закись азота; велосипед — звонок"
                                           : "бег (ещё раз — спринт); вода — быстрое плавание";
        case Action::R3: return vehicle || air ? "посмотреть назад" : "посмотреть назад; с прицелом — траектория броска";
        case Action::DpadUp:
            return air ? "мобильный телефон" : vehicle ? "телефон / взаимодействовать"
                                                       : "телефон / WAiNK; с прицелом — увеличить кратность";
        case Action::DpadDown:
            return vehicle || air ? "цель, мини-карта / быстрая навигация"
                                  : "цель, мини-карта / быстрая навигация; с прицелом — уменьшить кратность";
        case Action::DpadLeft:
            return air ? "радио / меню транспорта" : vehicle ? "левый поворотник / меню транспорта"
                                                             : "меню радио; с прицелом — сменить плечо";
        case Action::DpadRight:
            return air ? "меню партнёра" : vehicle ? "правый поворотник / меню партнёра"
                                                   : "меню партнёра; с прицелом — подорвать";
        case Action::Options: return "пауза";
        case Action::Create: return "скриншот / запись";
        case Action::Touchpad: return "карта";
        case Action::PS: return "меню PS5";
        case Action::Count: break;
    }
    return "";
}

std::string keyLabel(InputCode c);  // "ЛКМ", "Колесо", "↑": defined with the hint below

void drawBindings(App& app) {
    const Fonts& f = app.fonts;
    beginCard("##keys_edit", icon::Keyboard, "Раскладка клавиш", f);
    mutedText("Кликните по клавише и нажмите новую: клавиатура, кнопка или колёсико мыши. "
              "Esc — отмена, Backspace — очистить. Сохраняется сразу.",
              f);
    if (ImGui::BeginTabBar("##keys_classes")) {
        for (size_t i = 0; i < kControlClassCount; ++i) {
            const auto c = static_cast<ControlClass>(i);
            const std::string title = std::string(controlClassTitleRu(c)) + "##k";
            if (ImGui::BeginTabItem(title.c_str())) {
                if (app.keysClass != c) app.bindWaiting = false;
                app.keysClass = c;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    const Layout& layout = app.profile.layout(app.keysClass);
    ImGui::BeginChild("##keys_list", ImVec2(0, 560), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(14, 4));
    if (ImGui::BeginTable("##keys_table", 4,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_PadOuterX)) {
        ImGui::TableSetupColumn("Действие", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("Кнопка", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Клавиша", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Ещё", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        for (size_t i = 0; i < kActionCount; ++i) {
            const auto a = static_cast<Action>(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextWrapped("%s", capitalized(actionMeaningRu(app.keysClass, a)).c_str());
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
            ImGui::TextUnformatted(padButtonRu(a));
            ImGui::PopStyleColor();
            for (int s = 0; s < 2; ++s) {
                ImGui::TableNextColumn();
                const InputCode code = layout.bindings.slots[i][s];
                const bool waiting = app.bindWaiting && app.bindAction == a && app.bindSlot == s;
                std::string label = waiting ? "нажмите…" : code ? keyLabel(code) : "—";
                label += "##b" + std::to_string(i * 2 + s);
                if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, toVec4(col::Accent));
                if (ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0)) && !app.input->captured()) {
                    app.bindWaiting = true;
                    app.bindAction = a;
                    app.bindSlot = s;
                    app.bindSinceTick = GetTickCount64();
                    app.bindNote.clear();
                }
                if (waiting) ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    noteText(app.bindNote, col::Warn, f);
    endCard();
}

// ------------------------------------------------------------------ hint: controller buttons → your keys
// A picture to glance at while learning: the controller as GTA 6 draws it in its prompts, each
// button with the key it is on right now (follows the editor) and what it does in that mode.

constexpr Action kLeftStick = Action::MoveUp;  // row marker: movement keys (in the air: the mouse)
constexpr Action kRightStick = Action::Count;  // row marker: the camera, i.e. the mouse
constexpr ImU32 kWell = IM_COL32(13, 13, 17, 255);
constexpr ImU32 kKeycap = IM_COL32(48, 48, 60, 255);

void centeredText(ImDrawList* dl, ImVec2 c, ImU32 color, const char* text, ImFont* font) {
    ImGui::PushFont(font);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), color, text);
    ImGui::PopFont();
}

// The small button icon the game prints in its prompts.
void padGlyph(ImDrawList* dl, ImVec2 c, Action a, const Fonts& f) {
    const ImU32 ink = col::Text, dark = col::CardHi;  // dark: text on white glyphs
    const float r = 11;
    const auto ring = [&] { dl->AddCircle(c, r, ink, 24, 1.6f); };
    const auto dpad = [&](int lit) {  // 0 up, 1 down, 2 left, 3 right
        const ImVec2 arms[4][2] = {{{-3, -10}, {3, -3}}, {{-3, 3}, {3, 10}}, {{-10, -3}, {-3, 3}}, {{3, -3}, {10, 3}}};
        for (int i = 0; i < 4; ++i)
            dl->AddRectFilled(ImVec2(c.x + arms[i][0].x, c.y + arms[i][0].y),
                              ImVec2(c.x + arms[i][1].x, c.y + arms[i][1].y), i == lit ? ink : withAlpha(ink, 70),
                              1.5f);
    };
    const auto shoulder = [&](const char* name) {
        dl->AddRectFilled(ImVec2(c.x - 13, c.y - 9), ImVec2(c.x + 13, c.y + 9), ink, 4);
        centeredText(dl, c, dark, name, f.tiny);
    };
    switch (a) {
        case Action::Cross:
            ring();
            dl->AddLine(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), ink, 1.6f);
            dl->AddLine(ImVec2(c.x - 4.5f, c.y + 4.5f), ImVec2(c.x + 4.5f, c.y - 4.5f), ink, 1.6f);
            break;
        case Action::Circle: ring(); dl->AddCircle(c, 5, ink, 16, 1.6f); break;
        case Action::Square:
            ring();
            dl->AddRect(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), ink, 0, 0, 1.6f);
            break;
        case Action::Triangle:
            ring();
            dl->AddTriangle(ImVec2(c.x, c.y - 5.5f), ImVec2(c.x - 5.5f, c.y + 4), ImVec2(c.x + 5.5f, c.y + 4), ink, 1.6f);
            break;
        case Action::L1: shoulder("L1"); break;
        case Action::R1: shoulder("R1"); break;
        case Action::L2: shoulder("L2"); break;
        case Action::R2: shoulder("R2"); break;
        case Action::L3: ring(); centeredText(dl, c, ink, "L3", f.tiny); break;
        case Action::R3: ring(); centeredText(dl, c, ink, "R3", f.tiny); break;
        case kLeftStick: dl->AddCircleFilled(c, r, ink, 24); centeredText(dl, c, dark, "L", f.tiny); break;
        case kRightStick: dl->AddCircleFilled(c, r, ink, 24); centeredText(dl, c, dark, "R", f.tiny); break;
        case Action::DpadUp: dpad(0); break;
        case Action::DpadDown: dpad(1); break;
        case Action::DpadLeft: dpad(2); break;
        case Action::DpadRight: dpad(3); break;
        case Action::Touchpad: dl->AddRectFilled(ImVec2(c.x - 11, c.y - 6), ImVec2(c.x + 11, c.y + 6), ink, 3); break;
        case Action::Options: dl->AddRectFilled(ImVec2(c.x - 3, c.y - 8), ImVec2(c.x + 3, c.y + 8), ink, 3); break;
        case Action::Create: dl->AddRectFilled(ImVec2(c.x - 3, c.y - 6), ImVec2(c.x + 3, c.y + 6), ink, 3); break;
        case Action::PS: ring(); centeredText(dl, c, ink, "PS", f.tiny); break;
        default: centeredText(dl, c, ink, icon::Keyboard, f.body); break;  // our own modifiers
    }
}

// Line-art DualSense (outlines only), as on console control-scheme screens.
// s = 1: about 300 x 210 px; c = body centre. Leader lines point at padAnchor().
void drawDualSense(ImDrawList* dl, ImVec2 c, float s) {
    const auto P = [&](float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); };
    const ImU32 ink = withAlpha(col::Text, 225);
    const float t = std::max(1.2f, 1.6f * s);
    const auto body = [&] {
        dl->PathLineTo(P(60, -78));
        dl->PathBezierCubicCurveTo(P(110, -80), P(128, -60), P(136, -30));
        dl->PathBezierCubicCurveTo(P(150, 20), P(162, 80), P(140, 98));
        dl->PathBezierCubicCurveTo(P(122, 112), P(100, 100), P(90, 80));
        dl->PathBezierCubicCurveTo(P(78, 55), P(60, 42), P(0, 44));
        dl->PathBezierCubicCurveTo(P(-60, 42), P(-78, 55), P(-90, 80));
        dl->PathBezierCubicCurveTo(P(-100, 100), P(-122, 112), P(-140, 98));
        dl->PathBezierCubicCurveTo(P(-162, 80), P(-150, 20), P(-136, -30));
        dl->PathBezierCubicCurveTo(P(-128, -60), P(-110, -80), P(-60, -78));
    };
    for (float m : {-1.0f, 1.0f}) {  // triggers (L2/R2) behind bumpers (L1/R1)
        dl->AddRect(P(m < 0 ? -120 : 76, -102), P(m < 0 ? -76 : 120, -88), ink, 7 * s, 0, t);
        dl->AddRect(P(m < 0 ? -122 : 72, -88), P(m < 0 ? -72 : 122, -77), ink, 4 * s, 0, t);
    }
    body();
    dl->PathFillConcave(kWell);
    body();
    dl->PathStroke(ink, ImDrawFlags_Closed, t);
    dl->AddRect(P(-46, -76), P(46, -22), ink, 10 * s, 0, t);  // touchpad
    for (float m : {-1.0f, 1.0f})                               // Create / Options
        dl->AddRect(P(m * 58 - 2.5f, -72), P(m * 58 + 2.5f, -62), ink, 2 * s, 0, t);
    for (int i = -2; i <= 2; ++i) dl->AddCircleFilled(P(i * 5.0f, -14), 1.2f * s + 0.4f, ink, 8);  // speaker
    dl->AddCircle(P(0, 30), 5 * s, ink, 16, t);                                                     // PS button
    const ImVec2 arms[4][2] = {{{-97, -58}, {-87, -45}}, {{-97, -31}, {-87, -18}}, {{-112, -43}, {-99, -33}},
                               {{-85, -43}, {-72, -33}}};
    for (const auto& arm : arms) dl->AddRect(P(arm[0].x, arm[0].y), P(arm[1].x, arm[1].y), ink, 2.5f * s, 0, t);
    const float b = 8.5f * s, g = 3.6f * s;
    const ImVec2 tri = P(92, -52), cir = P(106, -38), cro = P(92, -24), sq = P(78, -38);
    for (const ImVec2 p : {tri, cir, cro, sq}) dl->AddCircle(p, b, ink, 24, t);
    dl->AddTriangle(ImVec2(tri.x, tri.y - g), ImVec2(tri.x - g, tri.y + g * 0.75f), ImVec2(tri.x + g, tri.y + g * 0.75f),
                    ink, t * 0.8f);
    dl->AddCircle(cir, g, ink, 16, t * 0.8f);
    dl->AddLine(ImVec2(cro.x - g, cro.y - g), ImVec2(cro.x + g, cro.y + g), ink, t * 0.8f);
    dl->AddLine(ImVec2(cro.x - g, cro.y + g), ImVec2(cro.x + g, cro.y - g), ink, t * 0.8f);
    dl->AddRect(ImVec2(sq.x - g, sq.y - g), ImVec2(sq.x + g, sq.y + g), ink, 0, 0, t * 0.8f);
    for (float x : {-36.0f, 36.0f}) {  // sticks
        dl->AddCircle(P(x, 14), 19 * s, ink, 40, t);
        dl->AddCircle(P(x, 14), 12 * s, ink, 32, t);
    }
}

// Where a button sits on drawDualSense's controller (same local units); false = nowhere (our modifiers).
bool padAnchor(Action a, ImVec2* at) {
    switch (a) {
        case Action::Touchpad: *at = {0, -50}; return true;
        case Action::Options: *at = {58, -67}; return true;
        case Action::Create: *at = {-58, -67}; return true;
        case Action::L1: *at = {-97, -82}; return true;
        case Action::R1: *at = {97, -82}; return true;
        case Action::L2: *at = {-98, -95}; return true;
        case Action::R2: *at = {98, -95}; return true;
        case Action::DpadUp: *at = {-92, -51}; return true;
        case Action::DpadDown: *at = {-92, -25}; return true;
        case Action::DpadLeft: *at = {-105, -38}; return true;
        case Action::DpadRight: *at = {-79, -38}; return true;
        case Action::Triangle: *at = {92, -52}; return true;
        case Action::Circle: *at = {106, -38}; return true;
        case Action::Cross: *at = {92, -24}; return true;
        case Action::Square: *at = {78, -38}; return true;
        case Action::L3:
        case kLeftStick: *at = {-36, 14}; return true;
        case Action::R3:
        case kRightStick: *at = {36, 14}; return true;
        case Action::PS: *at = {0, 30}; return true;
        default: return false;
    }
}

// Keys as people say them: "ЛКМ" rather than "Mouse1".
std::string keyLabel(InputCode c) {
    switch (c) {
        case code::MouseLeft: return "ЛКМ";
        case code::MouseRight: return "ПКМ";
        case code::MouseMiddle: return "Колесо";
        case code::MouseX1: return "Мышь 4";
        case code::MouseX2: return "Мышь 5";
        case code::WheelUp: return "Колесо ↑";
        case code::WheelDown: return "Колесо ↓";
        case vk::Up: return "↑";
        case vk::Down: return "↓";
        case vk::Left: return "←";
        case vk::Right: return "→";
        default: return inputCodeName(c);
    }
}

std::vector<std::string> boundKeys(const Layout& l, Action a) {
    std::vector<std::string> out;
    for (InputCode c : l.bindings.slots[static_cast<size_t>(a)])
        if (c != code::None) out.push_back(keyLabel(c));
    return out;
}

// "прыжок / лазание; бой — блок; …" -> "Прыжок / лазание": the main meaning only.
std::string shortMeaning(ControlClass c, Action a) {
    std::string s = actionMeaningRu(c, a);
    return capitalized(s.substr(0, s.find(';')).c_str());
}

struct HintRow {
    Action glyph;
    std::vector<std::string> keys;
    std::string text;
    float h = 0, keysW = 0;
    bool anchored = false;
    ImVec2 anchor;
};

void hintRows(ControlClass c, const Layout& l, std::vector<HintRow>& left, std::vector<HintRow>& right) {
    const auto add = [&](std::vector<HintRow>& side, Action a) {
        std::vector<std::string> keys = boundKeys(l, a);
        std::string text = shortMeaning(c, a);
        if (!keys.empty() && text != "—") side.push_back({a, std::move(keys), std::move(text)});
    };
    const auto firstKeys = [&](std::initializer_list<Action> actions) {
        std::vector<std::string> keys;
        for (Action a : actions)
            if (const auto k = boundKeys(l, a); !k.empty()) keys.push_back(k[0]);
        return keys;
    };
    for (Action a : {Action::Touchpad, Action::L2, Action::L1, Action::DpadUp, Action::DpadLeft, Action::DpadDown,
                     Action::DpadRight, Action::L3, Action::Create})
        add(left, a);
    for (Action a : {Action::Options, Action::R2, Action::R1, Action::Triangle, Action::Circle, Action::Cross,
                     Action::Square, Action::R3, Action::PS})
        add(right, a);
    if (l.mouseTarget == MouseTarget::LeftStick) {
        left.push_back({kLeftStick, {"Мышь"}, "Тангаж / крен"});
    } else {
        if (c == ControlClass::Ground || c == ControlClass::Water) {
            if (auto k = firstKeys({Action::MoveLeft, Action::MoveRight}); !k.empty())
                left.push_back({kLeftStick, std::move(k), "Руль"});
            if (auto k = firstKeys({Action::MoveUp, Action::MoveDown}); !k.empty())
                left.push_back({kLeftStick, std::move(k), "Наклон (мото, велосипед)"});
        } else if (auto k = firstKeys({Action::MoveUp, Action::MoveLeft, Action::MoveDown, Action::MoveRight});
                   !k.empty()) {
            left.push_back({kLeftStick, std::move(k), "Движение"});
        }
        right.push_back({kRightStick, {"Мышь"}, "Обзор"});
    }
    // Our own modifiers: no button on the controller, so no leader line.
    if (auto k = boundKeys(l, Action::Walk); !k.empty()) left.push_back({Action::Walk, std::move(k), "Шагом"});
    if (auto k = boundKeys(l, Action::Soft); !k.empty())
        left.push_back({Action::Soft, std::move(k), "Мягкий газ / тормоз"});
}

constexpr float kCapPad = 8, kCapGap = 4, kGlyphW = 26;

float keycapsWidth(const std::vector<std::string>& keys) {
    float w = 0;
    for (const std::string& k : keys) w += ImGui::CalcTextSize(k.c_str()).x + kCapPad * 2;
    return keys.empty() ? 0 : w + kCapGap * static_cast<float>(keys.size() - 1);
}

void drawKeycaps(ImDrawList* dl, float x, float midY, const std::vector<std::string>& keys) {
    const float h = ImGui::GetTextLineHeight() + 8;
    for (const std::string& k : keys) {
        const float w = ImGui::CalcTextSize(k.c_str()).x + kCapPad * 2;
        const ImVec2 a(x, midY - h * 0.5f), b(x + w, midY + h * 0.5f);
        dl->AddRectFilled(ImVec2(a.x, a.y + 2), ImVec2(b.x, b.y + 2), col::Line, 6);
        dl->AddRectFilled(a, b, kKeycap, 6);
        dl->AddText(ImVec2(a.x + kCapPad, a.y + 4), col::Text, k.c_str());
        x += w + kCapGap;
    }
}

float hintTextRoom(const HintRow& r, float colW) { return std::max(colW - r.keysW - kGlyphW - 30, 60.0f); }

void measureHintRow(HintRow& r, float colW, const Fonts& f) {
    ImGui::PushFont(f.semibold);
    r.keysW = keycapsWidth(r.keys);
    const float th = ImGui::CalcTextSize(r.text.c_str(), nullptr, false, hintTextRoom(r, colW)).y;  // long: 2 lines
    ImGui::PopFont();
    r.h = std::max(34.0f, th + 10);
    r.anchored = padAnchor(r.glyph, &r.anchor);
}

// Left column: keys on the outer edge, label right-aligned towards the controller, button icon,
// then a leader line to the button. The right column mirrors it.
void drawHintRow(ImDrawList* dl, const HintRow& r, ImVec2 p, float colW, bool leftSide, ImVec2 padC, float padS,
                 const Fonts& f) {
    const float mid = p.y + r.h * 0.5f;
    const float gx = leftSide ? p.x + colW - kGlyphW * 0.5f : p.x + kGlyphW * 0.5f;
    padGlyph(dl, ImVec2(gx, mid), r.glyph, f);

    ImGui::PushFont(f.semibold);
    drawKeycaps(dl, leftSide ? p.x : p.x + colW - r.keysW, mid, r.keys);
    ImGui::PopFont();

    const float room = hintTextRoom(r, colW);
    ImGui::PushFont(f.semibold);
    ImFont* font = ImGui::GetFont();
    const float fs = ImGui::GetFontSize(), lineH = ImGui::GetTextLineHeight();
    const char* text = r.text.c_str();
    const char* end = text + r.text.size();
    float ly = mid - ImGui::CalcTextSize(text, end, false, room).y * 0.5f;
    for (const char* s = text; s < end; ly += lineH) {  // line by line, so the left side can right-align
        const char* e = font->CalcWordWrapPositionA(fs / font->FontSize, s, end, room);
        if (e == s) {  // a word wider than the room: take one whole UTF-8 character and move on
            do {
                ++e;
            } while (e < end && (static_cast<unsigned char>(*e) & 0xC0) == 0x80);
        }
        const float lw = font->CalcTextSizeA(fs, FLT_MAX, 0, s, e).x;
        const float lx = leftSide ? gx - kGlyphW * 0.5f - 12 - lw : gx + kGlyphW * 0.5f + 12;
        dl->AddText(font, fs, ImVec2(lx, ly), col::Text, s, e);
        for (s = e; s < end && *s == ' '; ++s) {
        }
    }
    ImGui::PopFont();

    if (r.anchored) {  // out to the controller's side, then to the button
        const ImVec2 a(padC.x + r.anchor.x * padS, padC.y + r.anchor.y * padS);
        const float startX = leftSide ? p.x + colW + 4 : p.x - 4;
        const float elbow = padC.x + (leftSide ? -168.0f : 168.0f) * padS;
        const ImU32 line = withAlpha(col::Accent, 200);
        dl->AddLine(ImVec2(startX, mid), ImVec2(elbow, mid), line, 1.2f);
        dl->AddLine(ImVec2(elbow, mid), a, line, 1.2f);
        dl->AddCircleFilled(a, 3.0f, col::Accent, 12);
    }
}

void drawHint(App& app) {
    const Fonts& f = app.fonts;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x - 48, 1120.0f), 0));  // height fits the content
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(FLT_MAX, vp->WorkSize.y - 48));  // small screens: scroll
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(26, 22));
    const bool open = ImGui::BeginPopupModal("##hint", nullptr,
                                             ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    if (!open) return;

    const float closeW = 110;
    const float top = ImGui::GetCursorPosY();
    ImGui::PushFont(f.title);
    ImGui::TextUnformatted("Подсказка");
    ImGui::PopFont();
    ImGui::PushFont(f.tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
    ImGui::TextUnformatted("Кнопка из подсказок GTA 6 → ваша клавиша. Клавиши меняются на странице «Раскладка».");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - closeW, top));
    if (ghostButton("Закрыть  Esc", closeW) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::SetCursorPosY(top + 64);

    if (ImGui::BeginTabBar("##hint_tabs")) {
        for (size_t i = 0; i < kControlClassCount; ++i) {
            const auto c = static_cast<ControlClass>(i);
            const std::string title = std::string(controlClassTitleRu(c)) + "   F" + std::to_string(i + 1) + "##h";
            const ImGuiTabItemFlags flags =
                app.hintSelectTab && c == app.hintClass ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(title.c_str(), nullptr, flags)) {
                if (!app.hintSelectTab) app.hintClass = c;
                ImGui::EndTabItem();
            }
        }
        app.hintSelectTab = false;
        ImGui::EndTabBar();
    }

    std::vector<HintRow> left, right;
    hintRows(app.hintClass, app.profile.layout(app.hintClass), left, right);
    const float w = ImGui::GetContentRegionAvail().x;
    const float padS = std::clamp((w - 620) / 300.0f, 0.8f, 1.15f);
    const float colW = (w - 380 * padS) * 0.5f;
    float leftH = 0, rightH = 0;
    for (auto* side : {&left, &right}) {
        for (HintRow& r : *side) measureHintRow(r, colW, f);
        std::stable_sort(side->begin(), side->end(), [](const HintRow& a, const HintRow& b) {
            return a.anchored != b.anchored ? a.anchored : a.anchored && a.anchor.y < b.anchor.y;
        });  // rows in the order of their buttons, so leader lines don't cross
        float& total = side == &left ? leftH : rightH;
        for (const HintRow& r : *side) total += r.h + 4;
    }
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float areaTop = p.y + 18;
    const float areaH = std::max({leftH, rightH, 236 * padS});
    const ImVec2 padC(p.x + w * 0.5f, areaTop + areaH * 0.5f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    drawDualSense(dl, padC, padS);
    float y = areaTop + (areaH - leftH) * 0.5f;
    for (const HintRow& r : left) {
        drawHintRow(dl, r, ImVec2(p.x, y), colW, true, padC, padS, f);
        y += r.h + 4;
    }
    y = areaTop + (areaH - rightH) * 0.5f;
    for (const HintRow& r : right) {
        drawHintRow(dl, r, ImVec2(p.x + w - colW, y), colW, false, padC, padS, f);
        y += r.h + 4;
    }
    ImGui::Dummy(ImVec2(w, areaH + 36));

    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));
    struct Tip {
        const char* key;
        const char* what;
    };
    static const Tip tips[] = {{"F8", "захват вкл / выкл"}, {"F1–F5", "режим, как на вкладках"},
                               {"Home", "кнопка PS (держать)"}};
    for (const Tip& t : tips) {
        keycap(t.key, f.tiny);
        ImGui::SameLine(0, 8);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 3);
        ImGui::PushFont(f.tiny);
        ImGui::PushStyleColor(ImGuiCol_Text, toVec4(col::Muted));
        ImGui::TextUnformatted(t.what);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::SameLine(0, 28);
    }
    ImGui::NewLine();
    ImGui::EndPopup();
}

std::filesystem::path newRecordingDir() {
    PWSTR videos = nullptr;
    std::filesystem::path base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Videos, 0, nullptr, &videos))) base = videos;
    CoTaskMemFree(videos);
    if (base.empty()) base = appDataDir();
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf_s(name, L"rec-%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return base / L"RPlayKM" / name;
}

void drawRecord(App& app) {
    const Fonts& f = app.fonts;
    const bool on = app.recorder.recording();
    beginCard("##rec", icon::Record, "Запись для обучения", f);
    mutedText("Пишет видео 360p, вибрацию и триггеры от игры, чтобы научить программу саму понимать: пешком, "
              "машина, лодка, вертолёт или самолёт. Играйте как обычно, по 3–5 минут на каждый тип.",
              f);
    ImGui::Dummy(ImVec2(0, 2));
    if (!on) {
        ImGui::BeginDisabled(app.consoleOnly);
        const std::string label = std::string("●  Начать запись");
        if (primaryButton(label.c_str(), -FLT_MIN)) {
            app.recordError.clear();
            if (app.recorder.start(newRecordingDir(), &app.recordError)) app.link->requestKeyframe();
        }
        ImGui::EndDisabled();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, toVec4(withAlpha(col::Bad, 60)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(withAlpha(col::Bad, 100)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(withAlpha(col::Bad, 140)));
        const std::string label = std::string("■  Остановить запись");
        if (ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0))) app.recorder.stop();
        ImGui::PopStyleColor(3);
    }
    const Recorder::Stats s = app.recorder.stats();
    if (on || s.framesWritten) {
        char line[160];
        std::snprintf(line, sizeof(line), "%s%llu кадров · %.1f МБ · %llu событий%s", on ? "● " : "",
                      static_cast<unsigned long long>(s.framesWritten), s.bytesWritten / 1048576.0,
                      static_cast<unsigned long long>(s.events),
                      on && s.framesWritten == 0 ? " · жду опорный кадр от PS5…" : "");
        noteText(line, on ? col::Bad : col::Muted, f);
        const std::u8string folder8 = app.recorder.directory().u8string();
        const std::string folder(folder8.begin(), folder8.end());
        mutedText(folder.c_str(), f);
        if (!on && ghostButton((std::string(icon::Folder) + "  Открыть папку").c_str()))
            ShellExecuteW(nullptr, L"open", app.recorder.directory().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    noteText(app.recordError, col::Bad, f);
    endCard();
}

}  // namespace

void loadProfile(App& app) {
    app.profileWarnings.clear();
    std::ifstream f(app.profilePath, std::ios::binary);
    if (!f) {
        app.profile = defaultGtaProfile();
        saveProfile(app);
        app.saveNote.clear();
        return;
    }
    const std::string text((std::istreambuf_iterator<char>(f)), {});
    if (auto p = profileFromJson(text, &app.profileWarnings)) {
        app.profile = *p;
    } else {
        app.profile = defaultGtaProfile();
        app.profileWarnings.push_back("Профиль повреждён — загружен профиль по умолчанию (файл не тронут).");
    }
}

void saveProfile(App& app) {
    std::error_code ec;
    std::filesystem::create_directories(app.profilePath.parent_path(), ec);
    std::ofstream f(app.profilePath, std::ios::binary | std::ios::trunc);
    f << profileToJson(app.profile);
    app.saveNote = f ? "Сохранено" : "Не удалось сохранить";
}

void startCapture(App& app) {
    if (!app.consoleOnly && app.pairing) {
        const LinkState s = app.link->state();
        if (s == LinkState::Idle || s == LinkState::Lost) app.link->connect(*app.pairing);
    }
    app.input->setCapture(true);
}

void drawUi(App& app) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::Begin("RPlay K&M", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

    // Thin sunset line across the top: the one GTA 6 touch.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetWindowPos();
        const float w = ImGui::GetWindowWidth();
        dl->AddRectFilledMultiColor(p, ImVec2(p.x + w * 0.5f, p.y + 3), col::Accent, col::Accent2, col::Accent2,
                                    col::Accent);
        dl->AddRectFilledMultiColor(ImVec2(p.x + w * 0.5f, p.y), ImVec2(p.x + w, p.y + 3), col::Accent2,
                                    withAlpha(col::Accent2, 0), withAlpha(col::Accent2, 0), col::Accent2);
    }

    drawHeader(app);
    drawCaptureBanner(app);

    drawSidebar(app);
    ImGui::SameLine(0, 18);
    ImGui::BeginChild("##page", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    switch (app.page) {
        case Page::Home:
            if (ImGui::BeginTable("##home", 2, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn();
                drawPs5(app);
                ImGui::Dummy(ImVec2(0, 2));
                drawFirstSteps(app);
                ImGui::TableNextColumn();
                drawLive(app);
                if (!app.consoleOnly) {
                    ImGui::Dummy(ImVec2(0, 2));
                    drawNetwork(app);
                }
                ImGui::EndTable();
            }
            break;
        case Page::Mouse: drawMouse(app); break;
        case Page::Keys: drawBindings(app); break;
        case Page::Record: drawRecord(app); break;
        case Page::Help: drawKeys(app); break;
    }
    ImGui::EndChild();

    if (app.hintRequested) {  // opened here so the popup and its ID live in the main window
        ImGui::OpenPopup("##hint");
        app.hintRequested = false;
    }
    drawHint(app);
    ImGui::End();
}

void finishBinding(App& app, std::optional<InputCode> code, bool clear) {
    if (!app.bindWaiting) return;
    app.bindWaiting = false;
    if (!code && !clear) return;  // cancelled
    if (code && (*code == vk::F8 || (*code >= vk::F1 && *code <= vk::F1 + 4))) {
        app.bindNote = "F1–F5 и F8 заняты программой: выберите другую клавишу.";
        return;
    }
    if (code && inputCodeName(*code).empty()) {
        app.bindNote = "Эту клавишу пока нельзя назначить.";
        return;
    }
    auto& slot = app.profile.layout(app.keysClass).bindings.slots[static_cast<size_t>(app.bindAction)];
    slot[app.bindSlot] = clear ? code::None : *code;
    if (slot[0] == code::None && slot[1] != code::None) std::swap(slot[0], slot[1]);
    app.input->setProfile(app.profile);
    saveProfile(app);
}
