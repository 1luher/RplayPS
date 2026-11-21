// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <windows.h>

#include <imgui.h>

// Palette (sRGB). One dark theme, one accent.
namespace col {
constexpr ImU32 Bg = IM_COL32(9, 9, 12, 255);
constexpr ImU32 Card = IM_COL32(19, 19, 24, 255);
constexpr ImU32 CardHi = IM_COL32(28, 28, 35, 255);
constexpr ImU32 Line = IM_COL32(38, 38, 47, 255);
constexpr ImU32 Text = IM_COL32(236, 236, 241, 255);
constexpr ImU32 Muted = IM_COL32(140, 140, 155, 255);
constexpr ImU32 Accent = IM_COL32(255, 86, 136, 255);   // Vice-sunset pink: the only accent
constexpr ImU32 Accent2 = IM_COL32(255, 160, 72, 255);  // sunset orange: header gradient only
constexpr ImU32 AccentSoft = IM_COL32(255, 86, 136, 34);
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
constexpr const char* Record = "\xEE\x9F\x88";     // U+E7C8
constexpr const char* Folder = "\xEE\xA0\xB8";     // U+E838
constexpr const char* Home = "\xEE\xA0\x8F";       // U+E80F
constexpr const char* Help = "\xEE\xA2\x97";       // U+E897
constexpr const char* Globe = "\xEE\x9D\xB4";      // U+E774
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
