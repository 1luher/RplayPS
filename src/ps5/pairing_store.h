// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

// What a one-time pairing with the PS5 gives us. Kept encrypted on disk (DPAPI).
struct Pairing {
    std::string host;  // last known IP
    std::string name;  // console nickname
    std::string id;    // discovery host-id (MAC), finds the console again if DHCP moves it
    std::array<std::uint8_t, 16> registKey{};  // ASCII hex, as chiaki-ng's rp_regist_key
    std::array<std::uint8_t, 16> morning{};    // chiaki-ng's rp_key
    std::uint32_t target = 0;
};

std::vector<std::uint8_t> serializePairing(const Pairing& p);
std::optional<Pairing> deserializePairing(std::span<const std::uint8_t> bytes);

bool savePairing(const Pairing& p, const std::filesystem::path& file);
std::optional<Pairing> loadPairing(const std::filesystem::path& file);  // nullopt if missing or damaged

// %APPDATA%\RplayPS (created if missing).
std::filesystem::path appDataDir();
