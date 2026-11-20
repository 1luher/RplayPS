// SPDX-License-Identifier: AGPL-3.0-only
#include "core/quality.h"

#include <algorithm>
#include <cmath>

void QualityMeter::addPing(double ms) {
    probes_.push_back(ms < 0 ? 0 : ms);
    while (probes_.size() > kWindow) probes_.pop_front();
}

void QualityMeter::addMiss() {
    probes_.push_back(-1);
    while (probes_.size() > kWindow) probes_.pop_front();
}

void QualityMeter::addFrames(std::uint64_t received, std::uint64_t lost) {
    framesReceived_ += received;
    framesLost_ += lost;
}

void QualityMeter::reset() {
    probes_.clear();
    framesReceived_ = framesLost_ = 0;
}

NetQuality QualityMeter::snapshot() const {
    NetQuality q;
    double sum = 0, diffSum = 0, prev = -1;
    int answered = 0, diffs = 0, missed = 0;
    for (double p : probes_) {
        if (p < 0) {
            ++missed;
            continue;
        }
        sum += p;
        ++answered;
        if (prev >= 0) {
            diffSum += std::abs(p - prev);
            ++diffs;
        }
        prev = p;
    }
    q.samples = answered;
    if (!probes_.empty()) q.probeLossPct = 100.0 * missed / static_cast<double>(probes_.size());
    const std::uint64_t frames = framesReceived_ + framesLost_;
    if (frames) q.frameLossPct = 100.0 * static_cast<double>(framesLost_) / static_cast<double>(frames);
    if (answered == 0) return q;
    q.pingMs = sum / answered;
    q.jitterMs = diffs ? diffSum / diffs : 0;

    const double loss = std::max(q.probeLossPct, q.frameLossPct);
    if (q.pingMs < 10 && q.jitterMs < 3 && loss == 0)
        q.grade = NetGrade::Excellent;
    else if (q.pingMs < 20 && q.jitterMs < 8 && loss < 2)
        q.grade = NetGrade::Good;
    else if (q.pingMs < 40 && q.jitterMs < 20 && loss < 5)
        q.grade = NetGrade::Fair;
    else
        q.grade = NetGrade::Poor;
    return q;
}
