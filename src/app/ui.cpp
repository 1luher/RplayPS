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
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toVec4(IM_COL32(255, 150, 226, 255)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, toVec4(IM_COL32(228, 88, 196, 255)));
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
    dl->AddCircleFilled(c, radius, col::Well, 64);
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
    ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + width, p.y + h), col::Well, 10);
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
    dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w, y + h), col::Well, h);
    if (value) dl->AddRectFilled(ImVec2(p.x, y), ImVec2(p.x + w * value / 255.0f, y + h), col::Accent, h);
    ImGui::Dummy(ImVec2(w, 20));
}

// ------------------------------------------------------------------ sections

void drawHeader(App& app) {
    const Fonts& f = app.fonts;
    LinkState state;
    {
        std::lock_guard lock(app.statusMutex);
        state = app.linkState;
    }
    constexpr float kLogo = 44;
    const ImVec2 start = ImGui::GetCursorPos();
    drawLogo(ImGui::GetWindowDrawList(), ImGui::GetCursorScreenPos(), kLogo);
    ImGui::Dummy(ImVec2(kLogo, kLogo));
    ImGui::SameLine(0, 12);
    ImGui::PushFont(f.title);
    ImGui::SetCursorPosY(start.y + (kLogo - ImGui::GetTextLineHeight()) * 0.5f);
    ImGui::TextUnformatted("RPlay K&M");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::SetCursorPosY(start.y + kLogo * 0.5f - 6);
    mutedText("клавиатура и мышь для GTA 6 на PS5", f);

    // Right side: link state pill.
    const StateLook look = app.consoleOnly ? StateLook{"Без консоли", col::Muted} : stateLook(state);
    ImGui::PushFont(f.semibold);
    const float pillW = ImGui::CalcTextSize(look.text).x + 14 + 8 + 8 + 14;
    ImGui::PopFont();
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
    struct Row {
        const char* key;
        const char* what;
    };
    static const Row rows[] = {
        {"F8", "захват вкл / выкл"},
        {"WASD", "движение"},
        {"LAlt", "шаг"},
        {"ЛКМ / ПКМ", "R2 / L2"},
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
// Mirrors the game's Settings → Control scheme screen (Default scheme): the modes on the left, the
// controller in the middle, what every button does in that mode, and our key for every button.
// Labels are copied from the game's screens (GTA 6, 2026-11-21).

constexpr Action kLeftStick = Action::MoveUp;  // row marker: movement keys (in the air: the mouse)
constexpr Action kRightStick = Action::Count;  // row marker: the camera, i.e. the mouse

struct SchemeRow {
    Action a = Action::Count;
    const char* text = nullptr;  // nullptr ends the list
};

struct Scheme {
    const char* title;
    ControlClass edit, also;  // layouts this screen edits; also == edit when there is only one
    Action extra;             // our own modifier shown under the buttons, Action::Count = none
    const char* extraText;
    SchemeRow left[10], right[10];
};

constexpr ControlClass kFoot = ControlClass::Foot, kGround = ControlClass::Ground, kWater = ControlClass::Water,
                       kHeli = ControlClass::Heli, kPlane = ControlClass::Plane;

const Scheme kSchemes[] = {
    {"Пешком", kFoot, kFoot, Action::Walk, "ШАГ (НАШ МОДИФИКАТОР)",
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "СНАРЯЖЕНИЕ"}, {Action::L2, "ПРИЦЕЛИВАНИЕ / ВЗАИМОДЕЙСТВИЕ"},
      {Action::DpadUp, "ТЕЛЕФОН / ПРИЛОЖЕНИЕ WAINK"}, {Action::DpadLeft, "МЕНЮ РАДИО"},
      {Action::DpadDown, "ЦЕЛЬ / БЫСТРАЯ НАВИГАЦИЯ"}, {Action::DpadRight, "МЕНЮ ПАРТНЁРА"}, {Action::L3, "БЕГ"},
      {kLeftStick, "ДВИЖЕНИЕ"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "УКРЫТИЕ"}, {Action::R2, "ВЫСТРЕЛИТЬ"},
      {Action::Triangle, "ВЗАИМОДЕЙСТВОВАТЬ"}, {Action::Square, "БЛИЖНИЙ БОЙ"}, {Action::Circle, "ПРИСЕСТЬ / ОТДОХНУТЬ"},
      {Action::Cross, "ПРЫЖОК / ЛАЗАНИЕ"}, {Action::R3, "ПОСМОТРЕТЬ НАЗАД"}, {kRightStick, "ОБЗОР"}}},
    {"Прицеливание", kFoot, kFoot, Action::Count, nullptr,
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "СНАРЯЖЕНИЕ"}, {Action::L2, "ПРИЦЕЛИВАНИЕ / ВЗАИМОДЕЙСТВИЕ"},
      {Action::DpadUp, "УВЕЛИЧЕНИЕ КРАТНОСТИ"}, {Action::DpadLeft, "СМЕНИТЬ ПЛЕЧО"},
      {Action::DpadDown, "УМЕНЬШЕНИЕ КРАТНОСТИ"}, {Action::DpadRight, "ПОДОРВАТЬ"}, {Action::L3, "БЕГ"},
      {kLeftStick, "ДВИЖЕНИЕ"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "БЫСТРЫЙ БРОСОК / УКРЫТИЕ"}, {Action::R2, "ВЫСТРЕЛИТЬ"},
      {Action::Triangle, "БЫСТРАЯ СМЕНА ОРУЖИЯ"}, {Action::Square, "БЛИЖНИЙ БОЙ"}, {Action::Circle, "ПРИСЕСТЬ"},
      {Action::R3, "МАСШТАБ / ТРАЕКТОРИЯ БРОСКА"}, {kRightStick, "ОБЗОР"}}},
    {"Ближний бой", kFoot, kFoot, Action::Count, nullptr,
     {{Action::Touchpad, "КАРТА"}, {Action::L2, "ЗАХВАТ ЦЕЛИ / ДВИЖЕНИЕ БОКОМ"}, {kLeftStick, "ДВИЖЕНИЕ"}},
     {{Action::Options, "ПАУЗА"}, {Action::R2, "СХВАТИТЬ"}, {Action::Square, "УДАР КУЛАКОМ / УДАР НОГОЙ"},
      {Action::Cross, "БЛОК / УКЛОНЕНИЕ"}, {kRightStick, "ОБЗОР"}}},
    {"Наземный и водный транспорт", kGround, kWater, Action::Soft, "МЯГКИЙ ГАЗ / ТОРМОЗ (НАШ)",
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "КОБУРА / СНАРЯЖЕНИЕ"}, {Action::L2, "ТОРМОЗ / ЗАДНИЙ ХОД"},
      {Action::DpadUp, "ТЕЛЕФОН / ВЗАИМОДЕЙСТВОВАТЬ"}, {Action::DpadLeft, "ЛЕВЫЙ ПОВОРОТНИК / МЕНЮ ТРАНСПОРТА"},
      {Action::DpadDown, "ЦЕЛЬ / БЫСТРАЯ НАВИГАЦИЯ"}, {Action::DpadRight, "ПРАВЫЙ ПОВОРОТНИК / МЕНЮ ПАРТНЁРА"},
      {Action::L3, "КЛАКСОН / ЗАКИСЬ АЗОТА"}, {kLeftStick, "ПОВЕРНУТЬ (МОТО: НАКЛОН)"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "ВЫСТРЕЛИТЬ"}, {Action::R2, "УСКОРЕНИЕ"},
      {Action::Triangle, "ПОКИНУТЬ ТРАНСПОРТ"}, {Action::Square, "БЛИЖНИЙ БОЙ"}, {Action::Circle, "ФАРЫ / ПРИГНУТЬСЯ"},
      {Action::Cross, "РУЧНОЙ ТОРМОЗ"}, {Action::R3, "ПОСМОТРЕТЬ НАЗАД"}, {kRightStick, "ОБЗОР"}}},
    {"Воздушный транспорт", kHeli, kPlane, Action::Count, nullptr,
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "КОБУРА / СНАРЯЖЕНИЕ"}, {Action::L2, "СБРОСИТЬ ГАЗ"},
      {Action::DpadUp, "МОБИЛЬНЫЙ ТЕЛЕФОН"}, {Action::DpadLeft, "РАДИО / МЕНЮ ТРАНСПОРТА"},
      {Action::DpadDown, "ЦЕЛЬ / БЫСТРАЯ НАВИГАЦИЯ"}, {Action::DpadRight, "МЕНЮ ПАРТНЁРА"}, {Action::L3, "ШАССИ"},
      {kLeftStick, "ТАНГАЖ / КРЕН"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "ВЫСТРЕЛИТЬ"}, {Action::R2, "ПРИБАВИТЬ ГАЗ"},
      {Action::Triangle, "ПОКИНУТЬ ТРАНСПОРТ"}, {Action::Square, "КУРС ВЛЕВО"}, {Action::Circle, "КУРС ВПРАВО"},
      {Action::R3, "ПОСМОТРЕТЬ НАЗАД"}, {kRightStick, "ОБЗОР"}}},
    {"Велосипед", kGround, kWater, Action::Soft, "МЯГКИЕ ПЕДАЛИ / ТОРМОЗ (НАШ)",
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "КОБУРА / СНАРЯЖЕНИЕ"}, {Action::L2, "ТОРМОЗ / ЗАДНИЙ ХОД"},
      {Action::DpadUp, "ТЕЛЕФОН / ВЗАИМОДЕЙСТВОВАТЬ"}, {Action::DpadDown, "ЦЕЛЬ / БЫСТРАЯ НАВИГАЦИЯ"},
      {Action::DpadRight, "МЕНЮ ПАРТНЁРА"}, {Action::L3, "ЗВОНОК"}, {kLeftStick, "ПОВОРОТ / НАКЛОН"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "ВЫСТРЕЛИТЬ"}, {Action::R2, "ПЕДАЛИ / ДВИЖЕНИЕ ВПЕРЁД"},
      {Action::Triangle, "СЛЕЗТЬ"}, {Action::Square, "БЛИЖНИЙ БОЙ"}, {Action::Circle, "ПРЫЖОК"},
      {Action::Cross, "УСКОРЕНИЕ"}, {Action::R3, "ПОСМОТРЕТЬ НАЗАД"}, {kRightStick, "ОБЗОР"}}},
    {"Плавание", kFoot, kFoot, Action::Count, nullptr,
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "КОБУРА / СНАРЯЖЕНИЕ"}, {Action::L2, "ПРИЦЕЛИВАНИЕ / ДВИЖЕНИЕ БОКОМ"},
      {Action::DpadLeft, "РАДИО"}, {Action::DpadDown, "ЦЕЛЬ / БЫСТРАЯ НАВИГАЦИЯ"},
      {Action::DpadRight, "МЕНЮ ПАРТНЁРА"}, {Action::L3, "БЫСТРОЕ ПЛАВАНИЕ"}, {kLeftStick, "ДВИЖЕНИЕ"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "НЫРНУТЬ"}, {Action::R2, "ВЫСТРЕЛИТЬ"}, {Action::Square, "БЛИЖНИЙ БОЙ"},
      {Action::Cross, "ВЫНЫРНУТЬ"}, {Action::R3, "ПОСМОТРЕТЬ НАЗАД"}, {kRightStick, "ОБЗОР"}}},
    {"Парашют", kFoot, kFoot, Action::Count, nullptr,
     {{Action::Touchpad, "КАРТА"}, {Action::L1, "СНАРЯЖЕНИЕ"}, {Action::L2, "ЛЕВАЯ КЛЕВАНТА"},
      {Action::DpadUp, "ПРИЛОЖЕНИЕ WAINK"}, {Action::DpadLeft, "МЕНЮ РАДИО"}, {Action::DpadDown, "БЫСТРАЯ НАВИГАЦИЯ"},
      {Action::DpadRight, "МЕНЮ ПАРТНЁРА"}, {kLeftStick, "ПОВЕРНУТЬ"}},
     {{Action::Options, "ПАУЗА"}, {Action::R1, "ВЫСТРЕЛИТЬ"}, {Action::R2, "ПРАВАЯ КЛЕВАНТА"},
      {Action::Triangle, "ОТЦЕПИТЬ ПАРАШЮТ"}, {Action::Cross, "РАСКРЫТЬ ПАРАШЮТ"}, {Action::R3, "ПОСМОТРЕТЬ НАЗАД"},
      {kRightStick, "ОБЗОР"}}},
};
constexpr int kSchemeCount = static_cast<int>(std::size(kSchemes));

constexpr ImU32 kGlyphInk = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kGlyphDark = IM_COL32(64, 28, 118, 255);  // text on white glyphs and the selected mode

void centeredText(ImDrawList* dl, ImVec2 c, ImU32 color, const char* text, ImFont* font) {
    ImGui::PushFont(font);
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), color, text);
    ImGui::PopFont();
}

// The small button icon the game prints next to each label.
void padGlyph(ImDrawList* dl, ImVec2 c, Action a, const Fonts& f) {
    const float r = 11;
    const auto ring = [&] { dl->AddCircle(c, r, kGlyphInk, 24, 1.6f); };
    const auto dpad = [&](int lit) {  // 0 up, 1 down, 2 left, 3 right
        const ImVec2 arms[4][2] = {{{-3, -10}, {3, -3}}, {{-3, 3}, {3, 10}}, {{-10, -3}, {-3, 3}}, {{3, -3}, {10, 3}}};
        for (int i = 0; i < 4; ++i)
            dl->AddRectFilled(ImVec2(c.x + arms[i][0].x, c.y + arms[i][0].y),
                              ImVec2(c.x + arms[i][1].x, c.y + arms[i][1].y),
                              i == lit ? kGlyphInk : withAlpha(kGlyphInk, 80), 1.5f);
    };
    const auto shoulder = [&](const char* name) {
        dl->AddRectFilled(ImVec2(c.x - 13, c.y - 9), ImVec2(c.x + 13, c.y + 9), kGlyphInk, 4);
        centeredText(dl, c, kGlyphDark, name, f.tiny);
    };
    switch (a) {
        case Action::Cross:
            ring();
            dl->AddLine(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), kGlyphInk, 1.6f);
            dl->AddLine(ImVec2(c.x - 4.5f, c.y + 4.5f), ImVec2(c.x + 4.5f, c.y - 4.5f), kGlyphInk, 1.6f);
            break;
        case Action::Circle: ring(); dl->AddCircle(c, 5, kGlyphInk, 16, 1.6f); break;
        case Action::Square:
            ring();
            dl->AddRect(ImVec2(c.x - 4.5f, c.y - 4.5f), ImVec2(c.x + 4.5f, c.y + 4.5f), kGlyphInk, 0, 0, 1.6f);
            break;
        case Action::Triangle:
            ring();
            dl->AddTriangle(ImVec2(c.x, c.y - 5.5f), ImVec2(c.x - 5.5f, c.y + 4), ImVec2(c.x + 5.5f, c.y + 4), kGlyphInk,
                            1.6f);
            break;
        case Action::L1: shoulder("L1"); break;
        case Action::R1: shoulder("R1"); break;
        case Action::L2: shoulder("L2"); break;
        case Action::R2: shoulder("R2"); break;
        case Action::L3: ring(); centeredText(dl, c, kGlyphInk, "L3", f.tiny); break;
        case Action::R3: ring(); centeredText(dl, c, kGlyphInk, "R3", f.tiny); break;
        case kLeftStick: dl->AddCircleFilled(c, r, kGlyphInk, 24); centeredText(dl, c, kGlyphDark, "L", f.tiny); break;
        case kRightStick: dl->AddCircleFilled(c, r, kGlyphInk, 24); centeredText(dl, c, kGlyphDark, "R", f.tiny); break;
        case Action::DpadUp: dpad(0); break;
        case Action::DpadDown: dpad(1); break;
        case Action::DpadLeft: dpad(2); break;
        case Action::DpadRight: dpad(3); break;
        case Action::Touchpad: dl->AddRectFilled(ImVec2(c.x - 11, c.y - 6), ImVec2(c.x + 11, c.y + 6), kGlyphInk, 3); break;
        case Action::Options: dl->AddRectFilled(ImVec2(c.x - 3, c.y - 8), ImVec2(c.x + 3, c.y + 8), kGlyphInk, 3); break;
        default: centeredText(dl, c, kGlyphInk, icon::Keyboard, f.body); break;  // our own modifiers
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
    dl->PathFillConcave(withAlpha(col::Well, 120));
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

void startBinding(App& app, Action a, int slot) {
    if (app.input->captured()) return;
    app.bindWaiting = true;
    app.bindAction = a;
    app.bindSlot = slot;
    app.bindSinceTick = GetTickCount64();
    app.bindNote.clear();
}

// Our key(s) for one button: left click sets the first key, right click the second.
void keyChip(App& app, Action a, float width) {
    const auto& slots = app.profile.layout(kSchemes[app.keysSection].edit).bindings.slots[static_cast<size_t>(a)];
    const bool waiting = app.bindWaiting && app.bindAction == a;
    std::string text = waiting ? "нажмите…" : slots[0] ? inputCodeName(slots[0]) : "—";
    if (!waiting && slots[1]) text += " · " + inputCodeName(slots[1]);
    text += "##k" + std::to_string(static_cast<int>(a));
    if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, toVec4(col::Accent));
    ImGui::Button(text.c_str(), ImVec2(width, 0));
    if (waiting) ImGui::PopStyleColor();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) startBinding(app, a, 0);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) startBinding(app, a, 1);
    ImGui::SetItemTooltip("ЛКМ — клавиша, ПКМ — вторая клавиша");
}

void mouseChip(const char* text, float width) {
    ImGui::BeginDisabled();
    ImGui::Button(text, ImVec2(width, 0));
    ImGui::EndDisabled();
}

void rowKeys(App& app, Action a, float width) {
    const MouseTarget mouse = app.profile.layout(kSchemes[app.keysSection].edit).mouseTarget;
    if (a == kRightStick) return mouseChip(mouse == MouseTarget::RightStick ? "Мышь##rs" : "—##rs", width);
    if (a != kLeftStick) return keyChip(app, a, width);
    if (mouse == MouseTarget::LeftStick) return mouseChip("Мышь##ls", width);
    // Movement: four small chips, up / left / down / right (W A S D).
    const float gap = 4, w = (width - gap * 3) / 4;
    const auto& b = app.profile.layout(kSchemes[app.keysSection].edit).bindings;
    const Action dirs[] = {Action::MoveUp, Action::MoveLeft, Action::MoveDown, Action::MoveRight};
    for (int i = 0; i < 4; ++i) {
        if (i) ImGui::SameLine(0, gap);
        const Action d = dirs[i];
        const InputCode k = b.slots[static_cast<size_t>(d)][0];
        const bool waiting = app.bindWaiting && app.bindAction == d;
        const std::string text =
            (waiting ? std::string("…") : k ? inputCodeName(k) : std::string("—")) + "##m" + std::to_string(i);
        if (waiting) ImGui::PushStyleColor(ImGuiCol_Button, toVec4(col::Accent));
        if (ImGui::Button(text.c_str(), ImVec2(w, 0))) startBinding(app, d, 0);
        if (waiting) ImGui::PopStyleColor();
        ImGui::SetItemTooltip("%s", i == 0 ? "вперёд" : i == 1 ? "влево" : i == 2 ? "назад" : "вправо");
    }
}

// One label beside the controller. Our key sits on the outer edge, the button icon next to the
// controller, and a leader line runs from the icon to the button on the drawing.
struct PlacedRow {
    Action a;
    const char* text;
    float h = 0;        // row height (long labels wrap to two lines in the small font)
    bool compact = false;
    bool anchored = false;
    ImVec2 anchor;      // controller-local units, see padAnchor()
};

constexpr float kChipW = 92, kGlyphW = 26, kRowGap = 4;

float labelRoom(float colW) { return colW - kChipW - kGlyphW - 26; }

void measureRow(PlacedRow& r, float colW, const Fonts& f) {
    const float room = labelRoom(colW);
    ImGui::PushFont(f.semibold);
    r.compact = ImGui::CalcTextSize(r.text).x > room;
    ImGui::PopFont();
    ImGui::PushFont(r.compact ? f.tiny : f.semibold);
    const float th = ImGui::CalcTextSize(r.text, nullptr, false, room).y;
    ImGui::PopFont();
    r.h = std::max(34.0f, th + 10);
    r.anchored = padAnchor(r.a, &r.anchor);
}

void schemeRow(App& app, const PlacedRow& r, ImVec2 p, float colW, bool leftSide, ImVec2 padC, float padS) {
    const Fonts& f = app.fonts;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float mid = p.y + r.h * 0.5f;
    const float gx = leftSide ? p.x + colW - kGlyphW * 0.5f : p.x + kGlyphW * 0.5f;
    padGlyph(dl, ImVec2(gx, mid), r.a, f);

    const float room = labelRoom(colW);
    ImGui::PushFont(r.compact ? f.tiny : f.semibold);
    const ImVec2 ts = ImGui::CalcTextSize(r.text, nullptr, false, room);
    const float tx = leftSide ? (r.compact ? p.x + kChipW + 12 : gx - kGlyphW * 0.5f - 10 - ts.x) : gx + kGlyphW * 0.5f + 10;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(tx, mid - ts.y * 0.5f), col::Text, r.text, nullptr, room);
    ImGui::PopFont();

    if (r.anchored) {  // leader line: out to the controller's side, then to the button
        const ImVec2 a(padC.x + r.anchor.x * padS, padC.y + r.anchor.y * padS);
        const ImVec2 s(leftSide ? p.x + colW + 4 : p.x - 4, mid);
        const float elbow = padC.x + (leftSide ? -166.0f : 166.0f) * padS;
        const ImU32 line = withAlpha(col::Accent, 210);
        dl->AddLine(s, ImVec2(elbow, mid), line, 1.2f);
        dl->AddLine(ImVec2(elbow, mid), a, line, 1.2f);
        dl->AddCircleFilled(a, 3.0f, col::Accent, 12);
    }

    ImGui::SetCursorScreenPos(ImVec2(leftSide ? p.x : p.x + colW - kChipW, mid - ImGui::GetFrameHeight() * 0.5f));
    ImGui::PushID(leftSide ? 1 : 2);
    rowKeys(app, r.a, kChipW);
    ImGui::PopID();
}

// "Пешком" -> "ПЕШКОМ" (UTF-8 Cyrillic and Latin).
std::string upper(const char* s) {
    std::string out(s);
    auto* b = reinterpret_cast<unsigned char*>(out.data());
    for (size_t i = 0; i < out.size(); ++i) {
        if (b[i] >= 'a' && b[i] <= 'z') {
            b[i] = static_cast<unsigned char>(b[i] - 'a' + 'A');
        } else if (i + 1 < out.size() && b[i] == 0xD0 && b[i + 1] >= 0xB0 && b[i + 1] <= 0xBF) {  // а..п
            b[++i] -= 0x20;
        } else if (i + 1 < out.size() && b[i] == 0xD1 && b[i + 1] >= 0x80 && b[i + 1] <= 0x8F) {  // р..я
            b[i] = 0xD0;
            b[++i] += 0x20;
        } else if (i + 1 < out.size() && b[i] == 0xD1 && b[i + 1] == 0x91) {  // ё
            b[i] = 0xD0;
            b[++i] = 0x81;
        }
    }
    return out;
}

void drawSchemeList(App& app) {
    const Fonts& f = app.fonts;
    ImGui::PushFont(f.title);
    ImGui::TextUnformatted("ПРЕДПРОСМОТР");
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 2));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < kSchemeCount; ++i) {
        const bool on = app.keysSection == i;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const ImVec2 size(ImGui::GetContentRegionAvail().x, 40);
        const ImVec2 end(p.x + size.x, p.y + size.y);
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##mode", size) && !on) {
            app.bindWaiting = false;
            app.keysSection = i;
        }
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (on) {  // the game's selection: a white bar fading to pink, pink dot
            dl->AddRectFilledMultiColor(p, end, IM_COL32(255, 255, 255, 240), IM_COL32(246, 206, 240, 220),
                                        IM_COL32(246, 206, 240, 220), IM_COL32(255, 255, 255, 240));
        } else {
            if (hover) dl->AddRectFilled(p, end, IM_COL32(255, 255, 255, 22));
            dl->AddLine(ImVec2(p.x, end.y), end, col::Line);
        }
        ImGui::PushFont(on ? f.semibold : f.body);
        dl->AddText(ImVec2(p.x + 14, p.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f), on ? kGlyphDark : col::Text,
                    kSchemes[i].title);
        ImGui::PopFont();
        const ImVec2 dot(end.x - 20, p.y + size.y * 0.5f);
        dl->AddCircle(dot, 9, on ? withAlpha(kGlyphDark, 120) : withAlpha(col::Text, 150), 20, 1.5f);
        if (on) dl->AddCircleFilled(dot, 6, col::Accent, 20);
    }
    ImGui::Dummy(ImVec2(0, 4));
    char counter[16];
    std::snprintf(counter, sizeof(counter), "%d / %d", app.keysSection + 1, kSchemeCount);
    ImGui::PushFont(f.semibold);
    ImGui::TextUnformatted(counter);
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 6));

    const Scheme& s = kSchemes[app.keysSection];
    for (int j = 0; j < app.keysSection; ++j)
        if (kSchemes[j].edit == s.edit) {
            const std::string shared = std::string("Клавиши общие с «") + kSchemes[j].title +
                                       "»: в игре это один набор кнопок, меняются только действия.";
            mutedText(shared.c_str(), f);
            ImGui::Dummy(ImVec2(0, 2));
            break;
        }
    mutedText("ЛКМ по клавише — назначить, ПКМ — вторая клавиша. Esc — отмена, Backspace — очистить. "
              "Сохраняется сразу.",
              f);
    ImGui::Dummy(ImVec2(0, 4));
    if (ghostButton("По умолчанию", -FLT_MIN)) {
        const Profile def = defaultGtaProfile();
        app.profile.layout(s.edit).bindings = def.layout(s.edit).bindings;
        app.profile.layout(s.also).bindings = def.layout(s.also).bindings;
        app.bindWaiting = false;
        app.input->setProfile(app.profile);
        saveProfile(app);
    }
    noteText(app.bindNote, col::Warn, f);
}

void drawBindings(App& app) {
    const Fonts& f = app.fonts;
    ImGui::BeginChild("##modes", ImVec2(250, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    drawSchemeList(app);
    ImGui::EndChild();
    ImGui::SameLine(0, 22);
    ImGui::BeginChild("##scheme", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    const Scheme& s = kSchemes[app.keysSection];
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float cx = p.x + w * 0.5f;

    // Heading with a thin divider and a diamond, as on console control-scheme screens.
    const std::string heading = upper(s.title);
    ImGui::PushFont(f.title);
    const ImVec2 hs = ImGui::CalcTextSize(heading.c_str());
    dl->AddText(ImVec2(cx - hs.x * 0.5f, p.y), col::Text, heading.c_str());
    ImGui::PopFont();
    const float dy = p.y + hs.y + 12;
    dl->AddLine(ImVec2(cx - 150, dy), ImVec2(cx - 9, dy), col::Line, 1);
    dl->AddLine(ImVec2(cx + 9, dy), ImVec2(cx + 150, dy), col::Line, 1);
    dl->AddQuadFilled(ImVec2(cx, dy - 5), ImVec2(cx + 5, dy), ImVec2(cx, dy + 5), ImVec2(cx - 5, dy), col::Accent);

    // Controller in the middle, label columns either side; rows sorted by where their button is,
    // so the leader lines don't cross.
    const float padS = std::clamp((w - 640) / 300.0f, 0.75f, 1.05f);
    const float colW = (w - 380 * padS) * 0.5f;
    std::vector<PlacedRow> left, right;
    for (const SchemeRow& r : s.left)
        if (r.text) left.push_back({r.a, r.text});
    if (s.extra != Action::Count) left.push_back({s.extra, s.extraText});
    for (const SchemeRow& r : s.right)
        if (r.text) right.push_back({r.a, r.text});
    float leftH = 0, rightH = 0;
    for (auto* side : {&left, &right}) {
        for (PlacedRow& r : *side) measureRow(r, colW, f);
        std::stable_sort(side->begin(), side->end(), [](const PlacedRow& a, const PlacedRow& b) {
            return a.anchored != b.anchored ? a.anchored : a.anchored && a.anchor.y < b.anchor.y;
        });
        float& total = side == &left ? leftH : rightH;
        for (const PlacedRow& r : *side) total += r.h + kRowGap;
    }
    const float top = dy + 26;
    const float areaH = std::max({leftH, rightH, 230 * padS});
    const ImVec2 padC(cx, top + areaH * 0.5f);
    drawDualSense(dl, padC, padS);

    float y = top + (areaH - leftH) * 0.5f;
    for (const PlacedRow& r : left) {
        schemeRow(app, r, ImVec2(p.x, y), colW, true, padC, padS);
        y += r.h + kRowGap;
    }
    y = top + (areaH - rightH) * 0.5f;
    for (const PlacedRow& r : right) {
        schemeRow(app, r, ImVec2(p.x + w - colW, y), colW, false, padC, padS);
        y += r.h + kRowGap;
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w, top - p.y + areaH + 12));
    ImGui::EndChild();
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
    drawBackdrop(ImGui::GetBackgroundDrawList(), vp->Pos, vp->Size);
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
    ImGui::Begin("RPlay K&M", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleVar();

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
    const Scheme& s = kSchemes[app.keysSection];
    for (const ControlClass c : {s.edit, s.also}) {  // e.g. ground and water share one game screen
        auto& slot = app.profile.layout(c).bindings.slots[static_cast<size_t>(app.bindAction)];
        slot[app.bindSlot] = clear ? code::None : *code;
        if (slot[0] == code::None && slot[1] != code::None) std::swap(slot[0], slot[1]);
        if (s.also == s.edit) break;
    }
    app.input->setProfile(app.profile);
    saveProfile(app);
}
