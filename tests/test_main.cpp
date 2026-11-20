// SPDX-License-Identifier: AGPL-3.0-only
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>

#include "core/version.h"

TEST_CASE("smoke") { CHECK(std::string(rplayVersion()) == "0.1.0"); }
