// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include "core/quality.h"

using doctest::Approx;

TEST_CASE("quality_without_data_is_unknown") {
    QualityMeter q;
    CHECK(q.snapshot().grade == NetGrade::Unknown);
}

TEST_CASE("quality_ping_is_the_recent_average") {
    QualityMeter q;
    for (double ms : {4.0, 6.0, 5.0}) q.addPing(ms);
    CHECK(q.snapshot().pingMs == Approx(5.0));
}

TEST_CASE("quality_jitter_is_mean_change_between_pings") {
    QualityMeter q;
    for (double ms : {5.0, 15.0, 5.0}) q.addPing(ms);
    CHECK(q.snapshot().jitterMs == Approx(10.0));
}

TEST_CASE("quality_window_keeps_only_recent_pings") {
    QualityMeter q;
    for (int i = 0; i < 50; ++i) q.addPing(100);
    for (int i = 0; i < QualityMeter::kWindow; ++i) q.addPing(4);
    CHECK(q.snapshot().pingMs == Approx(4.0));
}

TEST_CASE("quality_loss_counts_missed_probes_and_lost_frames") {
    QualityMeter q;
    for (int i = 0; i < 9; ++i) q.addPing(5);
    q.addMiss();
    CHECK(q.snapshot().probeLossPct == Approx(10.0));
    q.addFrames(1, 0);
    q.addFrames(98, 1);  // 99 received + 1 lost = 1 lost out of 100
    CHECK(q.snapshot().frameLossPct == Approx(1.0));
}

TEST_CASE("quality_grades") {
    QualityMeter good;
    for (int i = 0; i < 10; ++i) good.addPing(4 + (i % 2));
    CHECK(good.snapshot().grade == NetGrade::Excellent);

    QualityMeter ok;
    for (int i = 0; i < 10; ++i) ok.addPing(i % 2 ? 14 : 24);  // jitter 10 ms
    CHECK(ok.snapshot().grade == NetGrade::Fair);

    QualityMeter bad;
    for (int i = 0; i < 10; ++i) bad.addPing(i % 2 ? 10 : 60);  // jitter 50 ms
    CHECK(bad.snapshot().grade == NetGrade::Poor);

    QualityMeter lossy;
    for (int i = 0; i < 8; ++i) lossy.addPing(4);
    lossy.addMiss();
    lossy.addMiss();  // 20 % of probes lost
    CHECK(lossy.snapshot().grade == NetGrade::Poor);
}

TEST_CASE("quality_reset") {
    QualityMeter q;
    q.addPing(5);
    q.addMiss();
    q.reset();
    CHECK(q.snapshot().grade == NetGrade::Unknown);
    CHECK(q.snapshot().probeLossPct == 0.0);
}
