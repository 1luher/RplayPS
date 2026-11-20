// SPDX-License-Identifier: AGPL-3.0-only
#include "ps5/pairing_store.h"

#include <windows.h>
#include <dpapi.h>
#include <shlobj.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

namespace {

constexpr std::uint8_t kFormatVersion = 1;

void putBytes(std::vector<std::uint8_t>& out, const void* p, size_t n) {
    const auto* b = static_cast<const std::uint8_t*>(p);
    out.insert(out.end(), b, b + n);
}

void putString(std::vector<std::uint8_t>& out, const std::string& s) {
    const auto n = static_cast<std::uint16_t>(std::min<size_t>(s.size(), 0xFFFF));
    putBytes(out, &n, 2);
    putBytes(out, s.data(), n);
}

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> in) : in_(in) {}
    bool bytes(void* dst, size_t n) {
        if (pos_ + n > in_.size()) return false;
        std::memcpy(dst, in_.data() + pos_, n);
        pos_ += n;
        return true;
    }
    bool string(std::string& s) {
        std::uint16_t n = 0;
        if (!bytes(&n, 2) || pos_ + n > in_.size()) return false;
        s.assign(reinterpret_cast<const char*>(in_.data() + pos_), n);
        pos_ += n;
        return true;
    }
    bool atEnd() const { return pos_ == in_.size(); }

private:
    std::span<const std::uint8_t> in_;
    size_t pos_ = 0;
};

}  // namespace

std::vector<std::uint8_t> serializePairing(const Pairing& p) {
    std::vector<std::uint8_t> out;
    out.push_back(kFormatVersion);
    putBytes(out, &p.target, 4);
    putBytes(out, p.registKey.data(), p.registKey.size());
    putBytes(out, p.morning.data(), p.morning.size());
    putString(out, p.host);
    putString(out, p.name);
    putString(out, p.id);
    return out;
}

std::optional<Pairing> deserializePairing(std::span<const std::uint8_t> bytes) {
    Reader r(bytes);
    std::uint8_t version = 0;
    Pairing p;
    if (!r.bytes(&version, 1) || version != kFormatVersion) return std::nullopt;
    if (!r.bytes(&p.target, 4) || !r.bytes(p.registKey.data(), p.registKey.size()) ||
        !r.bytes(p.morning.data(), p.morning.size()) || !r.string(p.host) || !r.string(p.name) ||
        !r.string(p.id) || !r.atEnd())
        return std::nullopt;
    return p;
}

bool savePairing(const Pairing& p, const std::filesystem::path& file) {
    auto plain = serializePairing(p);
    DATA_BLOB in{static_cast<DWORD>(plain.size()), plain.data()};
    DATA_BLOB out{};
    const BOOL ok = CryptProtectData(&in, L"RplayPS pairing", nullptr, nullptr, nullptr,
                                     CRYPTPROTECT_UI_FORBIDDEN, &out);
    SecureZeroMemory(plain.data(), plain.size());
    if (!ok) return false;
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream f(file, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(out.pbData), out.cbData);
    LocalFree(out.pbData);
    return static_cast<bool>(f);
}

std::optional<Pairing> loadPairing(const std::filesystem::path& file) {
    std::ifstream f(file, std::ios::binary);
    if (!f) return std::nullopt;
    std::vector<std::uint8_t> sealed((std::istreambuf_iterator<char>(f)), {});
    DATA_BLOB in{static_cast<DWORD>(sealed.size()), sealed.data()};
    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
        return std::nullopt;
    auto result = deserializePairing({out.pbData, out.cbData});
    SecureZeroMemory(out.pbData, out.cbData);
    LocalFree(out.pbData);
    return result;
}

std::filesystem::path appDataDir() {
    PWSTR roaming = nullptr;
    std::filesystem::path dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) dir = roaming;
    CoTaskMemFree(roaming);
    if (dir.empty()) dir = std::filesystem::temp_directory_path();
    dir /= L"RplayPS";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}
