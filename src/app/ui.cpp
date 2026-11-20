// SPDX-License-Identifier: AGPL-3.0-only
#include "app/ui.h"

#include <imgui.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>

#include "core/account_id.h"

namespace {

const char* stateName(LinkState s) {
    switch (s) {
        case LinkState::Idle: return "не подключено";
        case LinkState::Searching: return "поиск";
        case LinkState::Waking: return "пробуждение";
        case LinkState::Connecting: return "подключение";
        case LinkState::Connected: return "подключено";
        case LinkState::Lost: return "связь потеряна";
    }
    return "";
}

template <typename T>
bool ready(const std::future<T>& f) {
    return f.valid() && f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void drawStick(const char* label, Stick s) {
    const float radius = 50.0f;
    ImGui::BeginGroup();
    ImGui::TextUnformatted(label);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const ImVec2 c(p.x + radius, p.y + radius);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircle(c, radius, IM_COL32(150, 150, 150, 255), 48, 1.5f);
    dl->AddLine(ImVec2(c.x - radius, c.y), ImVec2(c.x + radius, c.y), IM_COL32(80, 80, 80, 255));
    dl->AddLine(ImVec2(c.x, c.y - radius), ImVec2(c.x, c.y + radius), IM_COL32(80, 80, 80, 255));
    dl->AddCircleFilled(ImVec2(c.x + s.x * radius, c.y + s.y * radius), 6.0f, IM_COL32(90, 200, 255, 255));
    ImGui::Dummy(ImVec2(radius * 2, radius * 2));
    ImGui::Text("%+.2f  %+.2f", s.x, s.y);
    ImGui::EndGroup();
}

void drawPs5(App& app) {
    LinkState state;
    std::string message;
    {
        std::lock_guard lock(app.statusMutex);
        state = app.linkState;
        message = app.linkMessage;
    }
    if (app.consoleOnly) {
        ImGui::TextWrapped("Режим без консоли: на PS5 ничего не отправляется.");
        return;
    }
    ImGui::Text("Состояние: %s", stateName(state));
    if (!message.empty()) ImGui::TextWrapped("%s", message.c_str());

    if (app.pairing) {
        ImGui::Text("PS5: %s (%s)", app.pairing->name.c_str(), app.pairing->host.c_str());
        const bool busy = state != LinkState::Idle && state != LinkState::Lost;
        if (!busy && ImGui::Button("Подключить")) app.link->connect(*app.pairing);
        if (busy && ImGui::Button("Отключить")) app.link->disconnect();
        ImGui::SameLine();
        if (ImGui::Button("Забыть PS5")) {
            app.link->disconnect();
            std::error_code ec;
            std::filesystem::remove(app.pairingPath, ec);
            app.pairing.reset();
        }
        return;
    }

    // Not paired yet.
    ImGui::TextWrapped("PS5 не привязана. На PS5: Настройки → Система → Дистанционное воспроизведение → "
                       "Привязать устройство. Введите IP, account ID и PIN с экрана.");
    const bool pairingBusy = app.pairing_job.valid();
    const bool findBusy = app.discover_job.valid();
    if (!findBusy && ImGui::Button("Найти PS5")) {
        Ps5Link* link = app.link;
        app.discover_job = std::async(std::launch::async, [link] { return link->discover(std::chrono::milliseconds(2500)); });
    }
    if (findBusy) ImGui::TextUnformatted("Ищу…");
    if (ready(app.discover_job)) {
        app.found = app.discover_job.get();
        if (app.found.empty()) app.pairError = "PS5 не найдена. Она включена и в той же сети? VPN выключен?";
    }
    for (const auto& c : app.found) {
        const std::string label = c.name + "  " + c.host + (c.ready ? "" : "  (спит)");
        if (ImGui::Selectable(label.c_str())) strncpy_s(app.hostField, c.host.c_str(), _TRUNCATE);
    }
    ImGui::InputText("IP", app.hostField, sizeof(app.hostField));
    ImGui::InputText("Account ID", app.accountField, sizeof(app.accountField));
    ImGui::InputText("PIN", app.pinField, sizeof(app.pinField), ImGuiInputTextFlags_CharsDecimal);
    if (!pairingBusy && ImGui::Button("Привязать")) {
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
    if (pairingBusy) ImGui::TextUnformatted("Привязываю… (до 30 секунд)");
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
    if (!app.pairError.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.4f, 1), "%s", app.pairError.c_str());
}

void drawMouse(App& app) {
    MouseSettings& m = app.profile.mouse;
    bool changed = false;
    changed |= ImGui::SliderFloat("Чувствительность", &m.sensitivity, 0.05f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    changed |= ImGui::SliderFloat("Вертикаль / горизонталь", &m.yRatio, 0.25f, 3.0f, "%.2f");
    changed |= ImGui::Checkbox("Инвертировать Y", &m.invertY);
    changed |= ImGui::SliderFloat("Кривая (степень)", &m.exponent, 0.5f, 3.0f, "%.2f");
    changed |= ImGui::SliderFloat("Анти-мёртвая зона", &m.antiDeadzone, 0.0f, 0.5f, "%.2f");
    changed |= ImGui::SliderFloat("Максимальный наклон", &m.outerLimit, 0.5f, 1.0f, "%.2f");
    changed |= ImGui::SliderInt("Окно скорости, мс", &m.windowMs, 2, 32);
    changed |= ImGui::SliderInt("Возврат в центр, мс", &m.recenterMs, 2, 100);
    changed |= ImGui::SliderFloat("Шаг (Alt), доля", &app.profile.walkScale, 0.1f, 1.0f, "%.2f");
    if (changed) {
        m.clamp();
        app.input->setProfile(app.profile);
        app.saveNote.clear();
    }
    if (ImGui::Button("Сохранить профиль")) saveProfile(app);
    if (!app.saveNote.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(app.saveNote.c_str());
    }
    for (const auto& w : app.profileWarnings) ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "%s", w.c_str());
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
    app.saveNote = f ? "Сохранено." : "Не удалось сохранить.";
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
    ImGui::Begin("RplayPS", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    const bool captured = app.input->captured();
    ImGui::TextColored(captured ? ImVec4(0.4f, 1, 0.5f, 1) : ImVec4(1, 1, 1, 1), "%s",
                       captured ? "ЗАХВАТ ВКЛЮЧЁН — F8 чтобы выключить"
                                : "F8 — захват. Затем переключите монитор на вход PS5.");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("PS5", ImGuiTreeNodeFlags_DefaultOpen)) drawPs5(app);
    if (ImGui::CollapsingHeader("Мышь", ImGuiTreeNodeFlags_DefaultOpen)) drawMouse(app);
    if (ImGui::CollapsingHeader("Визуализатор", ImGuiTreeNodeFlags_DefaultOpen)) {
        const InputStats s = app.input->stats();
        drawStick("Левый стик", s.lastPad.left);
        ImGui::SameLine(0, 40);
        drawStick("Правый стик", s.lastPad.right);
        ImGui::Text("Мышь: %.0f событий/с   Опоздание тика: %.0f мкс   L2 %u  R2 %u", s.mouseHz, s.tickJitterUs,
                    s.lastPad.l2, s.lastPad.r2);
    }
    ImGui::End();
}
