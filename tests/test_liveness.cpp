// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include "ps5/liveness.h"

TEST_CASE("liveness_three_missed_probes_mean_dead") {
    Liveness l;
    l.replied(true);
    l.missed();
    l.missed();
    CHECK_FALSE(l.dead());
    l.missed();
    CHECK(l.dead());
}

TEST_CASE("liveness_reply_resets_misses") {
    Liveness l;
    l.replied(true);
    l.missed();
    l.missed();
    l.replied(true);
    l.missed();
    l.missed();
    CHECK_FALSE(l.dead());
}

TEST_CASE("liveness_standby_reply_means_dead") {
    Liveness l;
    l.replied(false);  // the console answered from rest mode
    CHECK(l.dead());
}

TEST_CASE("liveness_never_answered_means_probing_is_not_usable") {
    // If the PS5 does not answer discovery during a session at all, misses prove nothing:
    // never declare the link dead on that basis (it would drop healthy sessions).
    Liveness l;
    for (int i = 0; i < 10; ++i) l.missed();
    CHECK_FALSE(l.dead());
}
