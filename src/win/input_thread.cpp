// SPDX-License-Identifier: AGPL-3.0-only
// Based on PS5-DirectView (AGPL-3.0), https://github.com/babymonie/PS5-DirectView (src/InputManager.cpp)
#include "win/input_thread.h"

#include <avrt.h>

namespace {
constexpr wchar_t kWindowClass[] = L"RplayPSInputSink";
constexpr UINT kMsgCapture = WM_APP + 1;
constexpr UINT kMsgProfile = WM_APP + 2;
constexpr UINT kMsgQuit = WM_APP + 3;
constexpr unsigned kVkF8 = 0x77;
constexpr UINT kMsgClass = WM_APP + 4;
constexpr unsigned kVkF1 = 0x70, kVkF5 = 0x74;  // temporary class keys until the detector exists
}  // namespace

std::optional<Cue> cueForCapture(CaptureEvent e) {
    switch (e) {
        case CaptureEvent::Captured: return Cue::CaptureOn;
        case CaptureEvent::Released: return Cue::CaptureOff;
        case CaptureEvent::FocusLost: return Cue::FocusLost;
        case CaptureEvent::LinkLost: break;
    }
    return std::nullopt;
}

DWORD stickyKeysFlagsWhileCaptured(DWORD original) {
    if (original & SKF_STICKYKEYSON) return original;  // the user relies on Sticky Keys: leave them
    return original & ~(SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY);
}

InputThread::InputThread(HWND mainWindow, Callbacks cb)
    : main_(mainWindow), cb_(std::move(cb)), builder_(std::make_unique<ClassPads>(Profile{})) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    qpcToMs_ = 1000.0 / static_cast<double>(f.QuadPart);
}

InputThread::~InputThread() { stop(); }

double InputThread::nowMs() const {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return static_cast<double>(t.QuadPart) * qpcToMs_;
}

void InputThread::start() {
    if (thread_.joinable()) return;
    ready_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    thread_ = std::thread([this] { threadMain(); });
    WaitForSingleObject(ready_, 5000);
}

void InputThread::stop() {
    if (!thread_.joinable()) return;
    if (window_) PostMessageW(window_, kMsgQuit, 0, 0);
    thread_.join();
    if (ready_) {
        CloseHandle(ready_);
        ready_ = nullptr;
    }
}

void InputThread::setCapture(bool on, CaptureEvent reason) {
    if (window_) PostMessageW(window_, kMsgCapture, on ? 1 : 0, static_cast<LPARAM>(reason));
}

bool InputThread::captured() const { return captured_; }

void InputThread::setProfile(const Profile& p) {
    {
        std::lock_guard lock(profileMutex_);
        pendingProfile_ = p;
    }
    if (window_) PostMessageW(window_, kMsgProfile, 0, 0);
}

InputStats InputThread::stats() const {
    std::lock_guard lock(statsMutex_);
    return stats_;
}

void InputThread::threadMain() {
    // Same scheduling class as games: keeps the 1 ms tick punctual.
    DWORD taskIndex = 0;
    HANDLE mmcss = AvSetMmThreadCharacteristicsW(L"Games", &taskIndex);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &InputThread::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);
    window_ = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    if (window_) SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    timer_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!timer_) timer_ = CreateWaitableTimerW(nullptr, FALSE, nullptr);
    rebuildPads();
    statsWindowStart_ = GetTickCount64();
    SetEvent(ready_);

    bool running = window_ != nullptr;
    while (running) {
        const DWORD handles = active_ && timer_ ? 1 : 0;
        const DWORD r = MsgWaitForMultipleObjectsEx(handles, &timer_, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (handles && r == WAIT_OBJECT_0) {
            // 1 ms tick: recentres the stick, ends wheel pulses, watches focus.
            if (active_ && GetForegroundWindow() != main_) applyCapture(false, CaptureEvent::FocusLost);
            if (active_) evaluate(true);
            continue;
        }
        bool hadInput = false;
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
                break;
            }
            if (msg.message == WM_INPUT) hadInput = true;
            DispatchMessageW(&msg);
            if (msg.message == kMsgQuit) running = false;
        }
        // One evaluation per batch of raw input (fast mice deliver bursts).
        if (running && hadInput && active_) evaluate(false);
    }

    if (active_) applyCapture(false, CaptureEvent::Released);
    if (timer_) {
        CloseHandle(timer_);
        timer_ = nullptr;
    }
    if (window_) {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
    if (mmcss) AvRevertMmThreadCharacteristics(mmcss);
}

LRESULT CALLBACK InputThread::wndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<InputThread*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self) {
        switch (message) {
            case WM_INPUT:
                if (self->active_) self->onRawInput(reinterpret_cast<HRAWINPUT>(lparam));
                break;  // DefWindowProc performs raw-input cleanup
            case kMsgCapture:
                if ((wparam != 0) != self->active_)
                    self->applyCapture(wparam != 0,
                                       wparam ? CaptureEvent::Captured : static_cast<CaptureEvent>(lparam));
                return 0;
            case kMsgProfile: self->rebuildPads(); return 0;
            case kMsgClass: self->applyClass(static_cast<ControlClass>(wparam)); return 0;
            case kMsgQuit: return 0;
            default: break;
        }
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void InputThread::applyCapture(bool on, CaptureEvent reason) {
    RAWINPUTDEVICE devices[2]{};
    devices[0].usUsagePage = 0x01;
    devices[0].usUsage = 0x02;  // mouse
    devices[1].usUsagePage = 0x01;
    devices[1].usUsage = 0x06;  // keyboard
    if (on) {
        if (GetForegroundWindow() != main_) {  // capture needs our window in front
            if (cb_.onCapture) cb_.onCapture(CaptureEvent::FocusLost);
            return;
        }
        for (auto& d : devices) {
            d.dwFlags = RIDEV_NOLEGACY | RIDEV_INPUTSINK;
            d.hwndTarget = window_;
        }
        if (!RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE))) return;
        builder_->releaseAll();
        mouse_.reset();
        sent_ = PadState{};
        RECT rc;  // keep the (hidden) cursor parked in the middle of our window
        if (GetWindowRect(main_, &rc)) {
            const LONG cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
            RECT clip{cx, cy, cx + 1, cy + 1};
            ClipCursor(&clip);
        }
        savedStickyKeys_ = {sizeof(STICKYKEYS), 0};
        if (SystemParametersInfoW(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &savedStickyKeys_, 0)) {
            STICKYKEYS during = savedStickyKeys_;
            during.dwFlags = stickyKeysFlagsWhileCaptured(savedStickyKeys_.dwFlags);
            if (during.dwFlags != savedStickyKeys_.dwFlags)
                stickyKeysChanged_ = SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &during, 0) != FALSE;
        }
        active_ = true;
        captured_ = true;
        LARGE_INTEGER due;
        due.QuadPart = -10'000;  // 1 ms, then every 1 ms
        if (timer_) SetWaitableTimer(timer_, &due, 1, nullptr, nullptr, FALSE);
        lastTickMs_ = nowMs();
        if (cb_.onCapture) cb_.onCapture(CaptureEvent::Captured);
        return;
    }

    for (auto& d : devices) {
        d.dwFlags = RIDEV_REMOVE;
        d.hwndTarget = nullptr;
    }
    RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));
    if (timer_) CancelWaitableTimer(timer_);
    ClipCursor(nullptr);
    if (stickyKeysChanged_) {  // restore the user's own Sticky Keys setting
        SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &savedStickyKeys_, 0);
        stickyKeysChanged_ = false;
    }
    active_ = false;
    captured_ = false;
    builder_->releaseAll();
    mouse_.reset();
    sent_ = PadState{};
    if (cb_.onPad) cb_.onPad(PadState{});  // release every button and centre both sticks
    {
        std::lock_guard lock(statsMutex_);
        stats_.lastPad = PadState{};
    }
    if (cb_.onCapture) cb_.onCapture(reason);
}

void InputThread::onRawInput(HRAWINPUT handle) {
    RAWINPUT raw{};
    UINT size = sizeof(raw);
    if (GetRawInputData(handle, RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) == static_cast<UINT>(-1)) return;
    const double now = nowMs();

    if (raw.header.dwType == RIM_TYPEMOUSE) {
        const RAWMOUSE& m = raw.data.mouse;
        if (!(m.usFlags & MOUSE_MOVE_ABSOLUTE) && (m.lLastX || m.lLastY)) {
            mouse_.addMotion(now, m.lLastX, m.lLastY);
            ++mouseEvents_;
        }
        const USHORT f = m.usButtonFlags;
        struct Btn {
            USHORT down, up;
            InputCode code;
        };
        static constexpr Btn kButtons[] = {
            {RI_MOUSE_LEFT_BUTTON_DOWN, RI_MOUSE_LEFT_BUTTON_UP, code::MouseLeft},
            {RI_MOUSE_RIGHT_BUTTON_DOWN, RI_MOUSE_RIGHT_BUTTON_UP, code::MouseRight},
            {RI_MOUSE_MIDDLE_BUTTON_DOWN, RI_MOUSE_MIDDLE_BUTTON_UP, code::MouseMiddle},
            {RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_4_UP, code::MouseX1},
            {RI_MOUSE_BUTTON_5_DOWN, RI_MOUSE_BUTTON_5_UP, code::MouseX2},
        };
        for (const Btn& b : kButtons) {
            if (f & b.down) builder_->setInput(b.code, true, now);
            if (f & b.up) builder_->setInput(b.code, false, now);
        }
        if (f & RI_MOUSE_WHEEL) {
            const short delta = static_cast<short>(m.usButtonData);
            if (delta > 0) builder_->setInput(code::WheelUp, true, now);
            if (delta < 0) builder_->setInput(code::WheelDown, true, now);
        }
    } else if (raw.header.dwType == RIM_TYPEKEYBOARD) {
        const RAWKEYBOARD& k = raw.data.keyboard;
        const bool up = (k.Flags & RI_KEY_BREAK) != 0;
        if (k.VKey == kVkF8) {  // capture toggle: never forwarded to the PS5
            if (!up) applyCapture(false, CaptureEvent::Released);
            return;
        }
        if (k.VKey >= kVkF1 && k.VKey <= kVkF5) {  // temporary class keys, never forwarded
            if (!up) applyClass(static_cast<ControlClass>(k.VKey - kVkF1));
            return;
        }
        const InputCode c = inputCodeFromRawKey(k.VKey, k.MakeCode, (k.Flags & RI_KEY_E0) != 0);
        if (c != code::None) builder_->setInput(c, !up, now);
    }
}

void InputThread::evaluate(bool timed) {
    const double now = nowMs();
    const PadState pad = builder_->build(now, mouse_.evaluate(now));  // active class decides the mapping
    if (pad != sent_) {
        sent_ = pad;
        if (cb_.onPad) cb_.onPad(pad);
    }
    std::lock_guard lock(statsMutex_);
    stats_.lastPad = pad;
    if (timed) {
        const double late = now - lastTickMs_ - 1.0;
        if (late >= 0.0 && late < 100.0) {
            jitterSum_ += late * 1000.0;
            ++tickSamples_;
        }
        lastTickMs_ = now;
    }
    const ULONGLONG tick = GetTickCount64();
    if (tick - statsWindowStart_ >= 1000) {
        const double secs = static_cast<double>(tick - statsWindowStart_) / 1000.0;
        stats_.mouseHz = mouseEvents_ / secs;
        stats_.tickJitterUs = tickSamples_ ? jitterSum_ / tickSamples_ : 0.0;
        mouseEvents_ = tickSamples_ = 0;
        jitterSum_ = 0;
        statsWindowStart_ = tick;
    }
}

void InputThread::setClass(ControlClass c) {
    if (window_) PostMessageW(window_, kMsgClass, static_cast<WPARAM>(c), 0);
}

void InputThread::rebuildPads() {
    std::lock_guard lock(profileMutex_);
    builder_ = std::make_unique<ClassPads>(pendingProfile_);  // key state starts fresh
    builder_->setActive(class_);
    mouse_.configure(builder_->activeLayout().mouse);
}

void InputThread::applyClass(ControlClass c) {
    if (static_cast<size_t>(c) >= kControlClassCount || c == builder_->active()) return;
    builder_->setActive(c);
    class_ = c;
    mouse_.configure(builder_->activeLayout().mouse);  // keeps recent motion: no jump
    if (cb_.onClass) cb_.onClass(c);
}
