// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/bindings.h"
#include "core/mouse_stick.h"

struct Profile {
    std::string name = "GTA 6";
    Bindings bindings;
    MouseSettings mouse;
    float walkScale = 0.5f;
};

// Starting layout modelled on GTA V's controls. To be checked in GTA 6.
Profile defaultGtaProfile();

std::string profileToJson(const Profile& p);
// Never throws. Unknown keys/actions, wrong types and extra keys are skipped with a
// warning (in Russian); numbers are clamped to their ranges. Unparseable JSON -> nullopt.
std::optional<Profile> profileFromJson(std::string_view json, std::vector<std::string>* warnings);
