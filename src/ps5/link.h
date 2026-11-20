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
    LinkState state() const;
    std::optional<Pairing> currentPairing() const;  // host may have been updated by DHCP

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
