// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include "ps5/pairing_store.h"

namespace {

Pairing samplePairing() {
    Pairing p;
    p.host = "192.168.1.50";
    p.name = "PS5-123";
    p.id = "A1B2C3D4E5F6";
    p.target = 1000100;
    for (int i = 0; i < 16; ++i) {
        p.registKey[i] = static_cast<std::uint8_t>('a' + i % 6);
        p.morning[i] = static_cast<std::uint8_t>(200 + i);
    }
    return p;
}

std::filesystem::path tempFile(const char* name) { return std::filesystem::temp_directory_path() / name; }

}  // namespace

TEST_CASE("pairing_serialize_roundtrip") {
    const Pairing p = samplePairing();
    auto back = deserializePairing(serializePairing(p));
    REQUIRE(back);
    CHECK(back->host == p.host);
    CHECK(back->name == p.name);
    CHECK(back->id == p.id);
    CHECK(back->target == p.target);
    CHECK(back->registKey == p.registKey);
    CHECK(back->morning == p.morning);
}

TEST_CASE("pairing_deserialize_rejects_truncated_data") {
    auto bytes = serializePairing(samplePairing());
    bytes.resize(bytes.size() - 3);
    CHECK_FALSE(deserializePairing(bytes));
    CHECK_FALSE(deserializePairing({}));
}

TEST_CASE("pairing_roundtrip_dpapi") {
    const auto path = tempFile("rplay_test_pairing.bin");
    const Pairing p = samplePairing();
    REQUIRE(savePairing(p, path));
    auto back = loadPairing(path);
    REQUIRE(back);
    CHECK(back->host == p.host);
    CHECK(back->registKey == p.registKey);
    CHECK(back->target == p.target);
    std::filesystem::remove(path);
}

TEST_CASE("pairing_file_is_not_plaintext") {
    const auto path = tempFile("rplay_test_pairing_plain.bin");
    REQUIRE(savePairing(samplePairing(), path));
    std::ifstream f(path, std::ios::binary);
    const std::string raw((std::istreambuf_iterator<char>(f)), {});
    f.close();
    CHECK(raw.find("192.168.1.50") == std::string::npos);
    std::filesystem::remove(path);
}

TEST_CASE("pairing_tampered_file_is_rejected") {
    const auto path = tempFile("rplay_test_pairing_bad.bin");
    REQUIRE(savePairing(samplePairing(), path));
    std::vector<char> data;
    {
        std::ifstream f(path, std::ios::binary);
        data.assign(std::istreambuf_iterator<char>(f), {});
    }
    data[data.size() / 2] ^= 0x5A;
    {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
    }
    CHECK_FALSE(loadPairing(path));
    std::filesystem::remove(path);
}

TEST_CASE("pairing_missing_file") { CHECK_FALSE(loadPairing(tempFile("rplay_no_such_pairing.bin"))); }
