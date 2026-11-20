// SPDX-License-Identifier: AGPL-3.0-only
#include "app/theme.h"

#include <dwmapi.h>

ImVec4 toVec4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

namespace {

ImFont* loadFont(const char* file, float size, const ImWchar* ranges, bool withIcons, float iconSize) {
    ImGuiIO& io = ImGui::GetIO();
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    ImFont* f = io.Fonts->AddFontFromFileTTF(file, size, &cfg, ranges);
    if (!f) return io.Fonts->AddFontDefault();
    if (withIcons) {
        static const ImWchar iconRanges[] = {0xE700, 0xF8FF, 0};
        ImFontConfig ic;
        ic.MergeMode = true;
        ic.GlyphMinAdvanceX = iconSize;
        ic.GlyphOffset = ImVec2(0, iconSize * 0.12f);
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\SegoeIcons.ttf", iconSize, &ic, iconRanges) ||
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segmdl2.ttf", iconSize, &ic, iconRanges);
    }
    return f;
}

}  // namespace

Fonts applyTheme(float dpi) {
    ImGuiIO& io = ImGui::GetIO();
    // Cyrillic plus the punctuation the messages use: — … « » → ●
    static ImVector<ImWchar> glyphs;
    if (glyphs.empty()) {
        ImFontGlyphRangesBuilder b;
        b.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        static const ImWchar extra[] = {0x2010, 0x2027, 0x2190, 0x2193, 0x00B7, 0x00B7, 0x25CF, 0x25CF, 0};
        b.AddRanges(extra);
        b.BuildRanges(&glyphs);
    }
    Fonts f;
    f.body = loadFont("C:\\Windows\\Fonts\\segoeui.ttf", 17.0f * dpi, glyphs.Data, true, 16.0f * dpi);
    f.semibold = loadFont("C:\\Windows\\Fonts\\seguisb.ttf", 17.0f * dpi, glyphs.Data, true, 16.0f * dpi);
    f.title = loadFont("C:\\Windows\\Fonts\\segoeuib.ttf", 26.0f * dpi, glyphs.Data, true, 24.0f * dpi);
    f.tiny = loadFont("C:\\Windows\\Fonts\\segoeui.ttf", 14.0f * dpi, glyphs.Data, false, 0);
    f.big = loadFont("C:\\Windows\\Fonts\\seguisb.ttf", 24.0f * dpi, glyphs.Data, false, 0);
    io.FontDefault = f.body;

    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowPadding = ImVec2(20, 18);
    s.FramePadding = ImVec2(12, 8);
    s.ItemSpacing = ImVec2(10, 10);
    s.ItemInnerSpacing = ImVec2(8, 6);
    s.WindowRounding = 0;
    s.ChildRounding = 14;
    s.FrameRounding = 8;
    s.GrabRounding = 8;
    s.PopupRounding = 10;
    s.ScrollbarRounding = 8;
    s.TabRounding = 8;
    s.GrabMinSize = 14;
    s.ScrollbarSize = 10;
    s.WindowBorderSize = 0;
    s.ChildBorderSize = 1;
    s.FrameBorderSize = 0;
    s.SeparatorTextBorderSize = 1;
    s.ScaleAllSizes(dpi);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = toVec4(col::Text);
    c[ImGuiCol_TextDisabled] = toVec4(col::Muted);
    c[ImGuiCol_WindowBg] = toVec4(col::Bg);
    c[ImGuiCol_ChildBg] = toVec4(col::Card);
    c[ImGuiCol_PopupBg] = toVec4(col::CardHi);
    c[ImGuiCol_Border] = toVec4(col::Line);
    c[ImGuiCol_FrameBg] = toVec4(IM_COL32(30, 30, 38, 255));
    c[ImGuiCol_FrameBgHovered] = toVec4(IM_COL32(38, 38, 48, 255));
    c[ImGuiCol_FrameBgActive] = toVec4(IM_COL32(44, 44, 56, 255));
    c[ImGuiCol_SliderGrab] = toVec4(col::Accent);
    c[ImGuiCol_SliderGrabActive] = toVec4(IM_COL32(255, 128, 166, 255));
    c[ImGuiCol_CheckMark] = toVec4(col::Accent);
    c[ImGuiCol_Button] = toVec4(IM_COL32(32, 32, 41, 255));
    c[ImGuiCol_ButtonHovered] = toVec4(IM_COL32(44, 44, 56, 255));
    c[ImGuiCol_ButtonActive] = toVec4(IM_COL32(52, 52, 66, 255));
    c[ImGuiCol_Header] = toVec4(col::AccentSoft);
    c[ImGuiCol_HeaderHovered] = toVec4(IM_COL32(255, 86, 136, 60));
    c[ImGuiCol_HeaderActive] = toVec4(IM_COL32(255, 86, 136, 90));
    c[ImGuiCol_Separator] = toVec4(col::Line);
    c[ImGuiCol_PlotHistogram] = toVec4(col::Accent);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = toVec4(col::Line);
    c[ImGuiCol_TextSelectedBg] = toVec4(IM_COL32(255, 86, 136, 80));
    c[ImGuiCol_NavHighlight] = toVec4(col::Accent);
    return f;
}

void styleTitleBar(HWND hwnd) {
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    const COLORREF caption = RGB(9, 9, 12), text = RGB(232, 235, 242);
    DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &text, sizeof(text));
}
