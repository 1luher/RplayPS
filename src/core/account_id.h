// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// PSN account ID as the PS5 expects it: 8 bytes, little-endian.
using AccountId = std::array<std::uint8_t, 8>;

// Accepts the decimal number or its base64 form (12 chars, trailing '=' optional).
// On failure returns nullopt and, if error is given, a message in Russian.
std::optional<AccountId> parseAccountId(std::string_view text, std::string* error);
std::string accountIdToBase64(const AccountId& id);
