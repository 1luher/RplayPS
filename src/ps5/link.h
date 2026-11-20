// SPDX-License-Identifier: AGPL-3.0-only
// Based on PS5-DirectView (AGPL-3.0), https://github.com/babymonie/PS5-DirectView
#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/account_id.h"
#include "core/pad.h"
#include "core/quality.h"
#include "ps5/pairing_store.h"

extern "C" {
#include <chiaki/controller.h>
}

struct FoundConsole {
    std::string name, host, id;
    bool ready = false;  // false = rest mode
};

enum class LinkState { Idle, Searching, Waking, Connecting, Connected, Lost };

// PadState -> chiaki-ng controller state (sticks to ±32767, +y down).
ChiakiControllerState padToChiaki(const PadState& pad);

// Input-only Remote Play link to a PS5 on the local network (no video, no audio).
class Ps5Link {
public:
    // Called from worker threads. Messages are in Russian.
    using StateFn = std::function<void(LinkState, const std::string& message)>;

    explicit Ps5Link(StateFn onState);
    ~Ps5Link();
    Ps5Link(const Ps5Link&) = delete;
    Ps5Link& operator=(const Ps5Link&) = delete;

    std::vector<FoundConsole> discover(std::chrono::milliseconds timeout);  // PS5 only
    // Blocking, up to ~30 s. The console must be fully on and showing the PIN.
    std::optional<Pairing> pair(const std::string& host, const AccountId& account, std::uint32_t pin,
                                std::string* errorRu);
    void connect(const Pairing& pairing);  // asynchronous: find, wake if needed, connect
    void disconnect();                      // sends a neutral state first, then stops
    void submit(const PadState& pad);       // thread-safe; ignored unless Connected

    // What the game sends back: the 360p H.264 video stream and controller feedback
    // (rumble, adaptive-trigger effects) as small JSON objects. Called on chiaki threads.
    // Set before connect(); used to record training data for the vehicle detector.
    struct Feedback {
        std::function<void(const std::uint8_t* data, size_t size)> onVideo;
        std::function<void(const char* type, const std::string& json)> onEvent;
        // Game sound, decoded from Opus: 16-bit PCM, interleaved. onAudioFormat comes first and again
        // whenever the format changes. Leave onAudio empty to drop the sound.
        std::function<void(unsigned channels, unsigned rate)> onAudioFormat;
        std::function<void(const std::int16_t* samples, size_t frames)> onAudio;
    };
    void setFeedback(Feedback fb);
    void requestKeyframe();  // asks the PS5 for a fresh IDR frame (e.g. when a recording starts)

    // Network quality while connected: ping/jitter from the liveness probes (every 2 s), video loss.
    NetQuality quality() const;
    LinkState state() const;
    std::optional<Pairing> currentPairing() const;  // host may have been updated by DHCP

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
