// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <string>

#include "core/account_id.h"

TEST_CASE("account_id_decimal_is_little_endian") {
    auto id = parseAccountId("1234567890123456789", nullptr);  // 0x112210F47DE98115
    REQUIRE(id);
    CHECK((*id)[0] == 0x15);
    CHECK((*id)[1] == 0x81);
    CHECK((*id)[7] == 0x11);
}

TEST_CASE("account_id_base64_roundtrip") {
    auto id = parseAccountId("1234567890123456789", nullptr);
    REQUIRE(id);
    auto b64 = accountIdToBase64(*id);
    CHECK(b64.size() == 12);
    CHECK(b64.back() == '=');
    auto again = parseAccountId(b64, nullptr);
    CHECK(again == id);
}

TEST_CASE("account_id_trims_spaces_and_accepts_missing_padding") {
    auto b64 = accountIdToBase64(*parseAccountId("42", nullptr));
    CHECK(parseAccountId("  " + b64 + " ", nullptr) == parseAccountId("42", nullptr));
    CHECK(parseAccountId(b64.substr(0, b64.size() - 1), nullptr) == parseAccountId("42", nullptr));
}

TEST_CASE("account_id_garbage_gives_error") {
    std::string e;
    CHECK_FALSE(parseAccountId("hello world", &e));
    CHECK_FALSE(e.empty());
    e.clear();
    CHECK_FALSE(parseAccountId("", &e));
    CHECK_FALSE(e.empty());
    e.clear();
    CHECK_FALSE(parseAccountId("99999999999999999999999", &e));  // > uint64
    CHECK_FALSE(e.empty());
}

TEST_CASE("pin_accepts_exactly_eight_digits") {
    CHECK(parsePin("12345678") == 12345678u);
    CHECK(parsePin(" 00001234 ") == 1234u);
    CHECK_FALSE(parsePin("--------"));
    CHECK_FALSE(parsePin("........"));
    CHECK_FALSE(parsePin("1234.567"));
    CHECK_FALSE(parsePin("1234567"));
    CHECK_FALSE(parsePin("123456789"));
    CHECK_FALSE(parsePin(""));
}

TEST_CASE("oauth_code_from_redirect_page_address") {
    CHECK(oauthCodeFromRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect?code=RGEf27&cid=cfd2610e-9112") ==
          "RGEf27");
    CHECK(oauthCodeFromRedirect("  https://remoteplay.dl.playstation.net/remoteplay/redirect?cid=x&code=Ab-9_c#top ") ==
          "Ab-9_c");
    CHECK(oauthCodeFromRedirect("vXh2Kq") == "vXh2Kq");  // the code alone
    CHECK_FALSE(oauthCodeFromRedirect("https://remoteplay.dl.playstation.net/remoteplay/redirect?cid=x"));
    CHECK_FALSE(oauthCodeFromRedirect("https://example.com/?code="));
    CHECK_FALSE(oauthCodeFromRedirect("https://example.com/?code=a b"));  // not a code: stops a bad request
    CHECK_FALSE(oauthCodeFromRedirect(""));
}
