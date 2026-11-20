// SPDX-License-Identifier: AGPL-3.0-only
// Based on PS5-DirectView (AGPL-3.0), https://github.com/babymonie/PS5-DirectView (src/InputManager.cpp)
#pragma once

#include <windows.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include "core/bindings.h"
#include "core/mouse_stick.h"
#include "core/pad.h"
#include "core/driving.h"
#include "core/profile.h"
#include "win/sound.h"

// LinkLost: released because the PS5 link dropped (its own alarm is already playing).
enum class CaptureEvent { Captured, Released, FocusLost, LinkLost };

// Which cue a capture change plays (none for LinkLost).
std::optional<Cue> cueForCapture(CaptureEvent e);

// STICKYKEYS flags to use while captured: the "press Shift 5 times" hotkey is switched off so
// tap-sprinting on Shift cannot pop up a dialog on the (invisible) PC screen. Left alone if the
// user has Sticky Keys turned on. Same pattern as Microsoft's "Disabling Shortcut Keys in Games".
DWORD stickyKeysFlagsWhileCaptured(DWORD original);

struct InputStats {
    double mouseHz = 0;       // raw mouse reports per second
    double tickJitterUs = 0;  // average lateness of the 1 ms tick
    PadState lastPad;
};

// Raw Input thread with a 1 ms tick. While captured it reads the mouse and keyboard directly,
// builds the controller state and calls onPad whenever it changes. F8 (in Raw Input) releases.
// Losing the foreground releases too. Releasing always calls onPad(PadState{}).
class InputThread {
public:
    struct Callbacks {
        std::function<void(const PadState&)> onPad;  // called on the input thread
        std::function<void(CaptureEvent)> onCapture; // called on the input thread
        std::function<void(ControlClass)> onClass;   // called on the input thread
    };

    InputThread(HWND mainWindow, Callbacks cb);
    ~InputThread();
    InputThread(const InputThread&) = delete;
    InputThread& operator=(const InputThread&) = delete;

    void start();
    void stop();
    // Any thread. When releasing, `reason` is what onCapture reports (Released or LinkLost).
    void setCapture(bool on, CaptureEvent reason = CaptureEvent::Released);
    bool captured() const;
    void setProfile(const Profile& p);  // any thread; applied on the input thread
    // Which layout is active (the vehicle detector will call this). Any thread.
    void setClass(ControlClass c);
    ControlClass currentClass() const { return class_; }
    InputStats stats() const;

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    void threadMain();
    void applyCapture(bool on, CaptureEvent reason);
    void onRawInput(HRAWINPUT h);
    void evaluate(bool timed);
    void applyClass(ControlClass c);
    void rebuildPads();
    double nowMs() const;

    HWND main_;
    Callbacks cb_;
    std::thread thread_;
    HWND window_ = nullptr;
    HANDLE timer_ = nullptr;
    HANDLE ready_ = nullptr;
    double qpcToMs_ = 0;

    std::atomic<bool> captured_{false};
    bool active_ = false;  // input-thread view of the capture

    std::mutex profileMutex_;
    Profile pendingProfile_;
    std::unique_ptr<ClassPads> builder_;
    std::atomic<ControlClass> class_{ControlClass::Foot};
    MouseStick mouse_;
    StickRamp steering_;  // smooth A/D steering in vehicles
    RadialAim radial_;    // weapon wheel / vehicle menu / partner menu
    PadRateLimiter limiter_;
    bool f8Down_ = false;
    PadState sent_;

    mutable std::mutex statsMutex_;
    InputStats stats_;
    unsigned mouseEvents_ = 0, tickSamples_ = 0;
    double jitterSum_ = 0, lastTickMs_ = 0;
    ULONGLONG statsWindowStart_ = 0;

    STICKYKEYS savedStickyKeys_{sizeof(STICKYKEYS), 0};
    bool stickyKeysChanged_ = false;
};
