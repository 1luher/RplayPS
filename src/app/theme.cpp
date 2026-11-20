// SPDX-License-Identifier: AGPL-3.0-only
#include "app/theme.h"

#include <dwmapi.h>

#include <algorithm>
#include <cmath>

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
    c[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);  // drawBackdrop paints it
    c[ImGuiCol_ChildBg] = toVec4(col::Card);
    c[ImGuiCol_PopupBg] = toVec4(IM_COL32(44, 18, 88, 245));
    c[ImGuiCol_Border] = toVec4(col::Line);
    c[ImGuiCol_FrameBg] = toVec4(col::Well);
    c[ImGuiCol_FrameBgHovered] = toVec4(IM_COL32(34, 14, 70, 200));
    c[ImGuiCol_FrameBgActive] = toVec4(IM_COL32(48, 20, 92, 220));
    c[ImGuiCol_SliderGrab] = toVec4(col::Accent);
    c[ImGuiCol_SliderGrabActive] = toVec4(IM_COL32(255, 156, 228, 255));
    c[ImGuiCol_CheckMark] = toVec4(col::Accent);
    c[ImGuiCol_Button] = toVec4(IM_COL32(40, 16, 82, 200));
    c[ImGuiCol_ButtonHovered] = toVec4(IM_COL32(64, 28, 120, 225));
    c[ImGuiCol_ButtonActive] = toVec4(IM_COL32(82, 36, 144, 240));
    c[ImGuiCol_Header] = toVec4(col::AccentSoft);
    c[ImGuiCol_HeaderHovered] = toVec4(IM_COL32(255, 112, 214, 64));
    c[ImGuiCol_HeaderActive] = toVec4(IM_COL32(255, 112, 214, 96));
    c[ImGuiCol_Separator] = toVec4(col::Line);
    c[ImGuiCol_PlotHistogram] = toVec4(col::Accent);
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab] = toVec4(col::Line);
    c[ImGuiCol_TextSelectedBg] = toVec4(IM_COL32(255, 112, 214, 80));
    c[ImGuiCol_NavHighlight] = toVec4(col::Accent);
    c[ImGuiCol_Tab] = toVec4(col::Card);
    c[ImGuiCol_TabHovered] = toVec4(IM_COL32(64, 28, 120, 225));
    c[ImGuiCol_TabSelected] = toVec4(IM_COL32(82, 36, 150, 240));
    c[ImGuiCol_TabSelectedOverline] = toVec4(col::Accent);
    c[ImGuiCol_TabDimmed] = c[ImGuiCol_Tab];
    c[ImGuiCol_TabDimmedSelected] = c[ImGuiCol_TabSelected];
    c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt] = toVec4(IM_COL32(255, 255, 255, 6));
    return f;
}

void styleTitleBar(HWND hwnd) {
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    const COLORREF caption = RGB(70, 32, 160), text = RGB(255, 255, 255);
    DwmSetWindowAttribute(hwnd, 35 /*DWMWA_CAPTION_COLOR*/, &caption, sizeof(caption));
    DwmSetWindowAttribute(hwnd, 36 /*DWMWA_TEXT_COLOR*/, &text, sizeof(text));
}

namespace {

ImVec2 lerp(ImVec2 a, ImVec2 b, float t) { return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }

ImVec2 bezier(ImVec2 a, ImVec2 b, ImVec2 c, float t) { return lerp(lerp(a, b, t), lerp(b, c, t), t); }

// A strip of varying width along a quadratic curve, filled as one polygon.
void strip(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec2 c, float w0, float w1, bool leafShape, ImU32 color) {
    constexpr int n = 16;
    ImVec2 left[n + 1], right[n + 1];
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / n;
        const ImVec2 p = bezier(a, b, c, t), q = bezier(a, b, c, std::min(t + 0.01f, 1.0f));
        const ImVec2 r = bezier(a, b, c, std::max(t - 0.01f, 0.0f));
        float dx = q.x - r.x, dy = q.y - r.y;
        const float len = std::sqrt(dx * dx + dy * dy) + 1e-4f;
        dx /= len;
        dy /= len;
        const float w = leafShape ? w0 * std::pow(std::max(0.0f, std::sin(3.14159f * (t * 0.9f + 0.1f))), 0.6f)
                                  : w0 + (w1 - w0) * t;
        left[i] = ImVec2(p.x - dy * w, p.y + dx * w);
        right[i] = ImVec2(p.x + dy * w, p.y - dx * w);
    }
    dl->PathClear();
    for (int i = 0; i <= n; ++i) dl->PathLineTo(left[i]);
    for (int i = n; i >= 0; --i) dl->PathLineTo(right[i]);
    dl->PathFillConcave(color);
}

// Fan palm like the ones in the game menu: a leaning trunk, drooping fronds with ragged leaflets.
void palm(ImDrawList* dl, ImVec2 base, float h, float lean, unsigned seed, ImU32 color) {
    const ImVec2 top(base.x + lean * h, base.y - h);
    strip(dl, base, ImVec2(base.x + lean * h * 0.15f, base.y - h * 0.5f), top, h * 0.032f, h * 0.016f, false,
          color);
    constexpr int fronds = 13;
    for (int i = 0; i < fronds; ++i) {
        seed = seed * 1103515245u + 12345u;
        const float jitter = static_cast<float>((seed >> 16) & 0xFF) / 255.0f - 0.5f;
        const float ang = -3.14159f * (0.02f + 0.96f * i / (fronds - 1)) + jitter * 0.25f;  // fan above
        const float len = h * (0.30f + 0.06f * jitter);
        const float dx = std::cos(ang), dy = std::sin(ang);
        const float droop = len * (0.35f + 0.55f * std::abs(dx));  // side fronds hang lower
        const ImVec2 mid(top.x + dx * len * 0.55f, top.y + dy * len * 0.55f);
        const ImVec2 tip(top.x + dx * len, top.y + dy * len + droop);
        strip(dl, top, mid, tip, len * 0.07f, 0, true, color);
        for (int k = 3; k <= 15; ++k) {  // leaflets hanging from the frond
            const float t = k / 16.0f;
            const ImVec2 p = bezier(top, mid, tip, t);
            const float l = len * 0.16f * (1.0f - t * 0.5f);
            dl->AddLine(p, ImVec2(p.x + dx * l * 0.35f, p.y + l), color, h * 0.006f + 1.0f);
        }
    }
    for (int i = 0; i < 5; ++i) {  // dead fronds under the crown
        const float dx = -0.6f + 0.3f * i;
        strip(dl, top, ImVec2(top.x + dx * h * 0.08f, top.y + h * 0.05f),
              ImVec2(top.x + dx * h * 0.10f, top.y + h * 0.16f), h * 0.018f, 0, true, color);
    }
}

}  // namespace

void drawBackdrop(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilledMultiColor(pos, end, col::Bg, col::BgEnd, col::BgEnd, col::Bg);
    const float s = std::min(size.x, size.y * 1.6f);
    struct P {
        float x, h, lean;
    };
    static constexpr P palms[] = {{0.58f, 0.42f, -0.08f}, {0.70f, 0.66f, 0.10f}, {0.84f, 0.80f, -0.05f},
                                  {0.95f, 0.56f, 0.12f}};
    unsigned seed = 7;
    for (const P& p : palms) palm(dl, ImVec2(pos.x + size.x * p.x, end.y + s * 0.02f), s * p.h * 0.62f, p.lean, seed++,
                                  col::Palm);
}
