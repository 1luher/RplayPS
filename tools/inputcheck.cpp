// SPDX-License-Identifier: AGPL-3.0-only
// inputcheck — diagnostic window for the input thread, no PS5 needed.
// F8 captures; the title shows both sticks, a few buttons and the mouse report rate.
#include <windows.h>

#include <cstdio>
#include <string>

#include "core/profile.h"
#include "win/input_thread.h"
#include "win/sound.h"

namespace {
InputThread* g_input = nullptr;
constexpr UINT_PTR kTitleTimer = 1;

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN:
            if (wp == VK_F8 && g_input && !g_input->captured()) g_input->setCapture(true);
            return 0;
        case WM_SETCURSOR:
            if (g_input && g_input->captured()) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        case WM_TIMER: {
            if (!g_input) return 0;
            const InputStats s = g_input->stats();
            wchar_t title[256];
            swprintf_s(title, L"%s L(%+.2f,%+.2f) R(%+.2f,%+.2f) R2=%u btn=%04X mouse=%.0f Hz jitter=%.0f us",
                       g_input->captured() ? L"[ЗАХВАТ]" : L"[F8 — захват]", s.lastPad.left.x, s.lastPad.left.y,
                       s.lastPad.right.x, s.lastPad.right.y, s.lastPad.r2, s.lastPad.buttons, s.mouseHz,
                       s.tickJitterUs);
            SetWindowTextW(hwnd, title);
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show) {
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"RplayPSInputCheck";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"inputcheck", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                1100, 200, nullptr, nullptr, inst, nullptr);
    ShowWindow(hwnd, show);

    InputThread input(hwnd, {nullptr, [](CaptureEvent e) {
                                 if (const auto cue = cueForCapture(e)) playCue(*cue);
                             }});
    input.setProfile(defaultGtaProfile());
    input.start();
    g_input = &input;
    SetTimer(hwnd, kTitleTimer, 50, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    g_input = nullptr;
    input.stop();
    return 0;
}
