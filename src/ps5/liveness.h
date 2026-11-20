// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// Decides when a connected PS5 has silently gone away. In LAN mode chiaki-ng never notices a
// dead peer by itself, so the link probes the console (discovery) every couple of seconds.
// Misses only count once the console has answered at least one probe during the session:
// if it never answers, probing tells us nothing and must not drop a healthy session.
class Liveness {
public:
    static constexpr int kMaxMisses = 3;

    void replied(bool ready) {
        answered_ = true;
        misses_ = 0;
        standby_ = !ready;
    }
    void missed() {
        if (answered_) ++misses_;
    }
    bool dead() const { return standby_ || misses_ >= kMaxMisses; }

private:
    bool answered_ = false;
    int misses_ = 0;
    bool standby_ = false;
};
