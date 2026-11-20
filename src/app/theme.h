// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <windows.h>

#include <imgui.h>

// Palette (sRGB), taken from the GTA 6 settings menu: violet -> magenta background with palm
// silhouettes, white text, a pink selection dot. Cards are translucent so the palms show through.
namespace col {
constexpr ImU32 Bg = IM_COL32(88, 42, 186, 255);       // background corners: top left (violet),
constexpr ImU32 BgBlue = IM_COL32(64, 126, 224, 255);  // top right (blue),
constexpr ImU32 BgPink = IM_COL32(222, 98, 186, 255);  // bottom right (pink),
constexpr ImU32 BgLow = IM_COL32(126, 50, 196, 255);   // bottom left
constexpr ImU32 Palm = IM_COL32(74, 30, 128, 255);     // palm silhouettes (opaque: leaflets overlap)
constexpr ImU32 Card = IM_COL32(30, 12, 64, 190);
constexpr ImU32 CardHi = IM_COL32(56, 24, 104, 215);
constexpr ImU32 Well = IM_COL32(18, 6, 40, 170);      // sunken areas: stick views, bars, tiles
constexpr ImU32 Line = IM_COL32(255, 255, 255, 40);
constexpr ImU32 Text = IM_COL32(255, 255, 255, 255);
constexpr ImU32 Muted = IM_COL32(208, 192, 236, 255);
constexpr ImU32 Accent = IM_COL32(255, 112, 214, 255);  // the game's pink selection dot
constexpr ImU32 AccentSoft = IM_COL32(255, 112, 214, 40);
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
// Window background: the menu gradient with palm silhouettes in the lower right.
void drawBackdrop(ImDrawList* dl, ImVec2 pos, ImVec2 size);
// The app logo (assets/logo.svg): "Rp" on a dark tile, size x size pixels at pos.
void drawLogo(ImDrawList* dl, ImVec2 pos, float size);

ImVec4 toVec4(ImU32 c);
