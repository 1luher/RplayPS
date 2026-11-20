// SPDX-License-Identifier: AGPL-3.0-only
#include "core/account_id.h"

#include <algorithm>
#include <cctype>

namespace {

constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string_view trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return s;
}

int base64Value(char c) {
    const char* p = std::find(kAlphabet, kAlphabet + 64, c);
    return p == kAlphabet + 64 ? -1 : static_cast<int>(p - kAlphabet);
}

void fail(std::string* error) {
    if (error) *error = "Account ID: ожидается число или 12 символов base64 (например, «Ab3dEf7hIj0=»).";
}

}  // namespace

std::optional<AccountId> parseAccountId(std::string_view text, std::string* error) {
    text = trim(text);
    if (text.empty()) {
        fail(error);
        return std::nullopt;
    }

    if (std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        std::uint64_t value = 0;
        for (char c : text) {
            const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
            if (value > (UINT64_MAX - digit) / 10) {
                if (error) *error = "Account ID: число слишком большое.";
                return std::nullopt;
            }
            value = value * 10 + digit;
        }
        AccountId id{};
        for (int i = 0; i < 8; ++i) id[i] = static_cast<std::uint8_t>(value >> (8 * i));
        return id;
    }

    // Base64: 8 bytes encode to 11 significant chars plus one '='.
    std::string_view body = text;
    if (!body.empty() && body.back() == '=') body.remove_suffix(1);
    if (body.size() != 11) {
        fail(error);
        return std::nullopt;
    }
    std::uint32_t buffer = 0;
    int bits = 0;
    AccountId id{};
    size_t out = 0;
    for (char c : body) {
        const int v = base64Value(c);
        if (v < 0) {
            fail(error);
            return std::nullopt;
        }
        buffer = (buffer << 6) | static_cast<std::uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (out < id.size()) id[out++] = static_cast<std::uint8_t>(buffer >> bits);
        }
    }
    if (out != id.size()) {
        fail(error);
        return std::nullopt;
    }
    return id;
}

std::string accountIdToBase64(const AccountId& id) {
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (std::uint8_t byte : id) {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out += kAlphabet[(buffer >> bits) & 0x3F];
        }
    }
    if (bits > 0) out += kAlphabet[(buffer << (6 - bits)) & 0x3F];
    out += '=';
    return out;
}
