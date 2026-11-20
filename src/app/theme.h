// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <windows.h>

#include <imgui.h>

// Palette (sRGB). One dark theme, one accent.
namespace col {
constexpr ImU32 Bg = IM_COL32(13, 15, 20, 255);
constexpr ImU32 Card = IM_COL32(22, 25, 33, 255);
constexpr ImU32 CardHi = IM_COL32(30, 34, 45, 255);
constexpr ImU32 Line = IM_COL32(40, 45, 58, 255);
constexpr ImU32 Text = IM_COL32(232, 235, 242, 255);
constexpr ImU32 Muted = IM_COL32(138, 145, 163, 255);
constexpr ImU32 Accent = IM_COL32(77, 124, 255, 255);
constexpr ImU32 AccentSoft = IM_COL32(77, 124, 255, 40);
constexpr ImU32 Good = IM_COL32(52, 211, 153, 255);
constexpr ImU32 Warn = IM_COL32(251, 191, 36, 255);
constexpr ImU32 Bad = IM_COL32(248, 113, 113, 255);
}  // namespace col

// Segoe Fluent Icons code points used in the UI.
namespace icon {
constexpr const char* Game = "\xEE\x9F\xBC";       // U+E7FC
constexpr const char* Mouse = "\xEE\xA5\xA2";      // U+E962
constexpr const char* Keyboard = "\xEE\x9D\xA5";   // U+E765
constexpr const char* Search = "\xEE\x9C\xA1";     // U+E721
constexpr const char* Link = "\xEE\x9C\x9B";       // U+E71B
constexpr const char* Save = "\xEE\x9D\x8E";       // U+E74E
constexpr const char* Sliders = "\xEE\xA7\xA9";    // U+E9E9
constexpr const char* Pulse = "\xEE\xA7\x99";      // U+E9D9
}  // namespace icon

struct Fonts {
    ImFont* body = nullptr;
    ImFont* semibold = nullptr;
    ImFont* title = nullptr;
    ImFont* tiny = nullptr;
    ImFont* big = nullptr;  // large numbers in stat tiles
};

// Loads Segoe UI (+ Cyrillic, punctuation) with Segoe Fluent Icons merged in, and styles ImGui.
Fonts applyTheme(float dpi);
// Dark title bar in the theme colour (Windows 11; harmless elsewhere).
void styleTitleBar(HWND hwnd);

ImVec4 toVec4(ImU32 c);
