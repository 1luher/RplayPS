// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstdint>
#include <deque>

enum class NetGrade { Unknown, Excellent, Good, Fair, Poor };

struct NetQuality {
    double pingMs = 0;        // average round trip to the PS5 (recent probes)
    double jitterMs = 0;      // average change between consecutive pings
    double probeLossPct = 0;  // probes without an answer (recent)
    double frameLossPct = 0;  // video frames lost on the way (whole session)
    int samples = 0;
    NetGrade grade = NetGrade::Unknown;
};

// Rates the network between the PC and the PS5 from periodic probe round-trips and video loss.
// Grades (for input over the local network):
//   Excellent: ping < 10 ms, jitter < 3 ms, no loss      Good: ping < 20, jitter < 8, loss < 2 %
//   Fair:      ping < 40, jitter < 20, loss < 5 %          Poor: anything worse
class QualityMeter {
public:
    static constexpr int kWindow = 15;  // probes kept for ping / jitter / probe loss

    void addPing(double ms);
    void addMiss();
    void addFrames(std::uint64_t received, std::uint64_t lost);
    void reset();
    NetQuality snapshot() const;

private:
    std::deque<double> probes_;  // ping in ms, or < 0 for a missed probe
    std::uint64_t framesReceived_ = 0, framesLost_ = 0;
};
