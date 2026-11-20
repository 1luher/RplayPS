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

constexpr float kPi = 3.14159265f;

ImVec2 lerp(ImVec2 a, ImVec2 b, float t) { return ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t); }

ImVec2 bezier(ImVec2 a, ImVec2 b, ImVec2 c, float t) { return lerp(lerp(a, b, t), lerp(b, c, t), t); }

ImVec2 tangent(ImVec2 a, ImVec2 b, ImVec2 c, float t) {
    const float x = 2 * (1 - t) * (b.x - a.x) + 2 * t * (c.x - b.x);
    const float y = 2 * (1 - t) * (b.y - a.y) + 2 * t * (c.y - b.y);
    const float l = std::max(std::sqrt(x * x + y * y), 1e-4f);
    return ImVec2(x / l, y / l);
}

struct Rng {  // tiny LCG: palms look the same on every frame
    unsigned s;
    float operator()() {
        s = s * 1664525u + 1013904223u;
        return static_cast<float>(s) / 4294967296.0f;
    }
};

// A strip whose width goes from w0 to w1 along a quadratic curve, filled as one polygon.
void strip(ImDrawList* dl, ImVec2 a, ImVec2 b, ImVec2 c, float w0, float w1, ImU32 color) {
    constexpr int n = 16;
    ImVec2 left[n + 1], right[n + 1];
    for (int i = 0; i <= n; ++i) {
        const float t = static_cast<float>(i) / n, w = w0 + (w1 - w0) * t;
        const ImVec2 p = bezier(a, b, c, t), d = tangent(a, b, c, t);
        left[i] = ImVec2(p.x - d.y * w, p.y + d.x * w);
        right[i] = ImVec2(p.x + d.y * w, p.y - d.x * w);
    }
    dl->PathClear();
    for (int i = 0; i <= n; ++i) dl->PathLineTo(left[i]);
    for (int i = n; i >= 0; --i) dl->PathLineTo(right[i]);
    dl->PathFillConcave(color);
}

// One coconut-palm frond: an arching stem that droops at the end, with thin leaflets hanging off
// both sides (longest in the middle).
void frond(ImDrawList* dl, ImVec2 c, float ang, float len, Rng& r, ImU32 color) {
    const float dx = std::cos(ang), dy = std::sin(ang), side = std::abs(dx);
    const ImVec2 p1(c.x + dx * len * 0.45f, c.y + dy * len * 0.45f - len * (0.12f + 0.18f * side));
    const ImVec2 p2(c.x + dx * len * 0.82f, c.y + dy * len * 0.82f + len * (0.18f + 0.55f * side));
    strip(dl, c, p1, p2, len * 0.012f, len * 0.002f, color);
    constexpr int kLeaflets = 44;
    for (int k = 0; k < kLeaflets; ++k) {
        const float t = 0.1f + 0.88f * static_cast<float>(k) / kLeaflets;
        const ImVec2 p = bezier(c, p1, p2, t), d = tangent(c, p1, p2, t), n(-d.y, d.x);
        for (float sd : {-1.0f, 1.0f}) {
            const float th = 0.75f + 0.15f * r();
            float vx = d.x * std::cos(th) + n.x * sd * std::sin(th);
            float vy = d.y * std::cos(th) + n.y * sd * std::sin(th) + 0.85f;  // leaflets hang
            const float vl = std::sqrt(vx * vx + vy * vy);
            vx /= vl;
            vy /= vl;
            const float l = len * 0.21f * std::pow(std::sin(kPi * (0.08f + 0.92f * t)), 0.6f) * (0.8f + 0.4f * r());
            const float wb = std::max(0.3f, l * 0.045f);
            dl->AddTriangleFilled(ImVec2(p.x - d.x * wb, p.y - d.y * wb), ImVec2(p.x + d.x * wb, p.y + d.y * wb),
                                  ImVec2(p.x + vx * l, p.y + vy * l), color);
        }
    }
}

// Coconut palm silhouette: slender curved trunk, crown of long arching fronds.
void palm(ImDrawList* dl, ImVec2 base, float h, float lean, unsigned seed, ImU32 color) {
    Rng r{seed};
    const ImVec2 top(base.x + lean * h, base.y - h);
    strip(dl, base, ImVec2(base.x + lean * h * 0.02f, base.y - h * 0.6f), top, h * 0.026f, h * 0.015f, color);
    static constexpr float kAngles[] = {-3.05f, -2.75f, -2.4f, -2.0f, -1.6f, -1.25f, -0.85f, -0.45f, -0.1f, 0.35f, 2.8f};
    for (const float a0 : kAngles) {
        const float a = a0 + (r() - 0.5f) * 0.25f;
        const float upright = a > 0 || a < -3 ? 0.55f : 0.62f + 0.38f * std::abs(std::cos(a));  // shorter going up
        frond(dl, top, a, h * (0.5f + 0.14f * r()) * upright, r, color);
    }
}

// Colours along the logo letters, top to bottom (as in assets/logo.svg).
ImU32 logoInk(float t) {
    struct Stop {
        float at;
        ImVec4 c;
    };
    static const Stop stops[] = {{0.0f, ImVec4(0.247f, 0.651f, 0.941f, 1)}, {0.45f, ImVec4(0.604f, 0.302f, 0.902f, 1)},
                                 {0.85f, ImVec4(1.0f, 0.435f, 0.784f, 1)}, {1.0f, ImVec4(1.0f, 0.635f, 0.722f, 1)}};
    t = std::clamp(t, 0.0f, 1.0f);
    for (int i = 1; i < 4; ++i)
        if (t <= stops[i].at) {
            const float u = (t - stops[i - 1].at) / (stops[i].at - stops[i - 1].at);
            const ImVec4 &a = stops[i - 1].c, &b = stops[i].c;
            return ImGui::ColorConvertFloat4ToU32(ImVec4(a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u, 1));
        }
    return ImGui::ColorConvertFloat4ToU32(stops[3].c);
}

}  // namespace

void drawLogo(ImDrawList* dl, ImVec2 pos, float size) {
    const float k = size / 120.0f;
    const auto P = [&](float x, float y) { return ImVec2(pos.x + (x - 2) * k, pos.y + (y - 6) * k); };  // translate(-2 -6)
    constexpr ImU32 tile = IM_COL32(0x1C, 0x0C, 0x3E, 255), outline = IM_COL32(0xFF, 0x6F, 0xC8, 255);
    dl->AddRectFilled(ImVec2(pos.x + 4 * k, pos.y + 4 * k), ImVec2(pos.x + 116 * k, pos.y + 116 * k), tile, 26 * k);
    const auto rOuter = [&] {
        dl->PathLineTo(P(14, 24)); dl->PathLineTo(P(44, 24));
        dl->PathBezierCubicCurveTo(P(57, 24), P(65, 32), P(65, 44)); dl->PathLineTo(P(65, 47));
        dl->PathBezierCubicCurveTo(P(65, 55), P(61, 60), P(55, 63)); dl->PathLineTo(P(67, 92));
        dl->PathLineTo(P(48, 92)); dl->PathLineTo(P(39, 68)); dl->PathLineTo(P(33, 68));
        dl->PathLineTo(P(33, 92)); dl->PathLineTo(P(14, 92));
    };
    const auto rCounter = [&] {
        dl->PathLineTo(P(33, 40)); dl->PathLineTo(P(42, 40));
        dl->PathBezierCubicCurveTo(P(45.5f, 40), P(47, 42), P(47, 45)); dl->PathLineTo(P(47, 47));
        dl->PathBezierCubicCurveTo(P(47, 50), P(45.5f, 52), P(42, 52)); dl->PathLineTo(P(33, 52));
    };
    const auto pOuter = [&] {
        dl->PathLineTo(P(72, 46)); dl->PathLineTo(P(96, 46));
        dl->PathBezierCubicCurveTo(P(104.5f, 46), P(110, 51.5f), P(110, 60)); dl->PathLineTo(P(110, 78));
        dl->PathBezierCubicCurveTo(P(110, 86.5f), P(104.5f, 92), P(96, 92)); dl->PathLineTo(P(88, 92));
        dl->PathLineTo(P(88, 108)); dl->PathLineTo(P(72, 108));
    };
    const auto pCounter = [&] {
        dl->PathLineTo(P(88, 60)); dl->PathLineTo(P(91.5f, 60));
        dl->PathBezierCubicCurveTo(P(93, 60), P(94, 61), P(94, 62.5f)); dl->PathLineTo(P(94, 75.5f));
        dl->PathBezierCubicCurveTo(P(94, 77), P(93, 78), P(91.5f, 78)); dl->PathLineTo(P(88, 78));
    };
    const float pen = std::max(1.0f, 3 * k);
    const auto letter = [&](const auto& shape) {  // pink outline behind, gradient letter on top
        shape();
        dl->PathStroke(outline, ImDrawFlags_Closed, pen);
        const int first = dl->VtxBuffer.Size;
        shape();
        dl->PathFillConcave(IM_COL32_WHITE);
        for (int i = first; i < dl->VtxBuffer.Size; ++i) {
            ImDrawVert& v = dl->VtxBuffer[i];
            const ImU32 alpha = v.col & IM_COL32_A_MASK;  // keep the anti-aliased fringe
            v.col = (logoInk((v.pos.y - pos.y - 14 * k) / (90 * k)) & ~IM_COL32_A_MASK) | alpha;
        }
    };
    letter(rOuter);
    letter(pOuter);
    const auto counter = [&](const auto& hole) {  // counters: the tile shows through, with the outline
        hole();
        dl->PathFillConcave(tile);
        hole();
        dl->PathStroke(outline, ImDrawFlags_Closed, pen * 0.5f);
    };
    counter(rCounter);
    counter(pCounter);
}

void drawBackdrop(ImDrawList* dl, ImVec2 pos, ImVec2 size) {
    const ImVec2 end(pos.x + size.x, pos.y + size.y);
    dl->AddRectFilledMultiColor(pos, end, col::Bg, col::BgBlue, col::BgPink, col::BgLow);
    struct P {
        float x, h, lean;
    };
    // Heights are fractions of the window height; crowns stay inside the window.
    static constexpr P palms[] = {{0.60f, 0.46f, -0.08f}, {0.72f, 0.68f, 0.07f}, {0.85f, 0.78f, -0.06f},
                                  {0.96f, 0.58f, 0.10f}};
    unsigned seed = 21;
    for (const P& p : palms) palm(dl, ImVec2(pos.x + size.x * p.x, end.y + 4), size.y * p.h, p.lean, seed++, col::Palm);
}
