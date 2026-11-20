// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <windows.h>

#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/profile.h"
#include "ps5/link.h"
#include "app/theme.h"
#include "win/input_thread.h"

// Messages posted to the main window from worker threads.
constexpr UINT kMsgLinkState = WM_APP + 10;  // wParam = LinkState
constexpr UINT kMsgCapture = WM_APP + 11;    // wParam = CaptureEvent

struct App {
    HWND window = nullptr;
    bool consoleOnly = false;  // --console-only: no PS5, visualiser only
    Fonts fonts;

    Ps5Link* link = nullptr;
    InputThread* input = nullptr;

    std::filesystem::path pairingPath, profilePath;
    std::optional<Pairing> pairing;
    Profile profile;
    std::vector<std::string> profileWarnings;

    std::mutex statusMutex;  // the link reports from its worker thread
    LinkState linkState = LinkState::Idle;
    std::string linkMessage;

    // Pair / find run in the background so the window keeps drawing.
    std::future<std::pair<std::optional<Pairing>, std::string>> pairing_job;  // result, error
    std::future<std::vector<FoundConsole>> discover_job;
    std::string pairError;
    std::vector<FoundConsole> found;
    char hostField[64] = "";
    char accountField[64] = "";
    char pinField[16] = "";
    std::string saveNote;
};

void loadProfile(App& app);
void saveProfile(App& app);
// Called on F8 when not captured: connects if needed, then captures input.
void startCapture(App& app);
// Draws the whole window contents; call between ImGui::NewFrame() and ImGui::Render().
void drawUi(App& app);
