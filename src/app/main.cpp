// SPDX-License-Identifier: AGPL-3.0-only
// RplayPS: play PS5 games with keyboard and mouse over input-only Remote Play.
// D3D11 / ImGui setup follows imgui/examples/example_win32_directx11 (MIT).
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>

#include <imgui.h>
#include <backends/imgui_impl_dx11.h>
#include <backends/imgui_impl_win32.h>

#include "app/ui.h"
#include "win/sound.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
UINT g_resizeW = 0, g_resizeH = 0;
App* g_app = nullptr;

void createRenderTarget() {
    ID3D11Texture2D* back = nullptr;
    g_swapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) {
        g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
        back->Release();
    }
}

void cleanupRenderTarget() {
    if (g_rtv) {
        g_rtv->Release();
        g_rtv = nullptr;
    }
}

bool createDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate = {60, 1};
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                               D3D11_SDK_VERSION, &sd, &g_swapChain, &g_device, &got, &g_context);
    if (hr == DXGI_ERROR_UNSUPPORTED)  // no GPU driver: fall back to WARP
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                           &sd, &g_swapChain, &g_device, &got, &g_context);
    if (FAILED(hr)) return false;
    createRenderTarget();
    return true;
}

void cleanupDevice() {
    cleanupRenderTarget();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    g_swapChain = nullptr;
    g_context = nullptr;
    g_device = nullptr;
}

LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return true;
    switch (msg) {
        case WM_SIZE:
            if (wp != SIZE_MINIMIZED) {
                g_resizeW = LOWORD(lp);
                g_resizeH = HIWORD(lp);
            }
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_F8 && g_app && !g_app->input->captured()) startCapture(*g_app);
            break;
        case WM_SETCURSOR:
            if (g_app && g_app->input->captured()) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        case kMsgLinkState: {
            if (!g_app) return 0;
            const auto s = static_cast<LinkState>(wp);
            if (s == LinkState::Connected) {
                playCue(Cue::LinkUp);
                if (auto updated = g_app->link->currentPairing();
                    updated && g_app->pairing && updated->host != g_app->pairing->host) {
                    g_app->pairing = updated;  // DHCP moved the console
                    savePairing(*updated, g_app->pairingPath);
                }
            } else if (s == LinkState::Lost) {
                playCue(Cue::LinkLost);
                g_app->input->setCapture(false, CaptureEvent::LinkLost);  // no second cue over the alarm
            }
            return 0;
        }
        case kMsgCapture:
            if (const auto cue = cueForCapture(static_cast<CaptureEvent>(wp))) playCue(*cue);
            return 0;
        case WM_SYSCOMMAND:
            if ((wp & 0xfff0) == SC_KEYMENU) return 0;  // Alt is a game key: no window menu
            break;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool hasArg(const wchar_t* name) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool found = false;
    for (int i = 1; i < argc; ++i)
        if (lstrcmpiW(argv[i], name) == 0) found = true;
    LocalFree(argv);
    return found;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, wndProc, 0, 0, inst, nullptr, LoadCursorW(nullptr, IDC_ARROW), nullptr,
                   nullptr, L"RplayPS", nullptr};
    RegisterClassExW(&wc);
    const float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"RplayPS", WS_OVERLAPPEDWINDOW, 100, 100, static_cast<int>(720 * dpi),
                              static_cast<int>(860 * dpi), nullptr, nullptr, inst, nullptr);
    if (!createDevice(hwnd)) {
        cleanupDevice();
        MessageBoxW(nullptr, L"Не удалось запустить Direct3D 11.", L"RplayPS", MB_ICONERROR);
        return 1;
    }
    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpi);
    // Cyrillic plus the punctuation the messages use: — … « » →
    static ImVector<ImWchar> glyphs;
    {
        ImFontGlyphRangesBuilder builder;
        builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());
        static const ImWchar extra[] = {0x2010, 0x2027, 0x2190, 0x2193, 0};
        builder.AddRanges(extra);
        builder.BuildRanges(&glyphs);
    }
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f * dpi, nullptr, glyphs.Data))
        io.Fonts->AddFontDefault();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    App app;
    app.window = hwnd;
    app.consoleOnly = hasArg(L"--console-only");
    const auto dir = appDataDir();
    app.pairingPath = dir / L"pairing.bin";
    app.profilePath = dir / L"profiles" / L"gta6.json";
    loadProfile(app);
    app.pairing = loadPairing(app.pairingPath);

    Ps5Link link([hwnd, &app](LinkState s, const std::string& message) {
        {
            std::lock_guard lock(app.statusMutex);
            app.linkState = s;
            app.linkMessage = message;
        }
        PostMessageW(hwnd, kMsgLinkState, static_cast<WPARAM>(s), 0);
    });
    InputThread input(hwnd, {[&link, &app](const PadState& pad) {
                                 if (!app.consoleOnly) link.submit(pad);
                             },
                             [hwnd](CaptureEvent e) { PostMessageW(hwnd, kMsgCapture, static_cast<WPARAM>(e), 0); }});
    app.link = &link;
    app.input = &input;
    input.setProfile(app.profile);
    input.start();
    g_app = &app;
    if (app.pairing && !app.consoleOnly) link.connect(*app.pairing);

    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;
        if (g_resizeW && g_resizeH) {
            cleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            createRenderTarget();
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        drawUi(app);
        ImGui::Render();
        const float clear[4] = {0.08f, 0.08f, 0.1f, 1.0f};
        g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_context->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapChain->Present(1, 0);
    }

    g_app = nullptr;
    input.stop();       // releases capture: a neutral pad goes to the PS5
    link.disconnect();  // neutral state first, then stop
    if (app.pairing_job.valid()) app.pairing_job.wait();
    if (app.discover_job.valid()) app.discover_job.wait();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDevice();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, inst);
    return 0;
}
