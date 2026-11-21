// SPDX-License-Identifier: AGPL-3.0-only
// Endpoints, client and parameters as in chiaki-ng gui/include/psnaccountid.h (AGPL-3.0).
#include "ps5/psn_login.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <string_view>

#include <nlohmann/json.hpp>

namespace {

// The PS Remote Play Windows app's OAuth client (ba495a24-…), as chiaki-ng uses it:
// "Basic " + base64("<client id>:<client secret>").
constexpr wchar_t kAuthHeader[] =
    L"Authorization: Basic YmE0OTVhMjQtODE4Yy00NzJiLWIxMmQtZmYyMzFjMWI1NzQ1Om12YWlaa1JzQXNJMUlCa1k=";
constexpr wchar_t kHost[] = L"auth.api.sonyentertainmentnetwork.com";
constexpr wchar_t kTokenPath[] = L"/2.0/oauth/token";
constexpr char kRedirect[] = "https://remoteplay.dl.playstation.net/remoteplay/redirect";
constexpr char kScope[] = "psn:clientapp referenceDataService:countryConfig.read "
                          "pushNotification:webSocket.desktop.connect sessionManager:remotePlaySession.system.update";

std::string urlEncode(std::string_view s) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += ch;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h) WinHttpCloseHandle(h);
    }
};

// One HTTPS request to Sony's auth server. false: no answer at all (network, TLS, timeout).
bool request(const wchar_t* method, const std::wstring& path, const std::string& body, const std::wstring& headers,
             DWORD* status, std::string* reply) {
    Handle session{WinHttpOpen(L"RPlayKM/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.h) return false;
    WinHttpSetTimeouts(session.h, 10000, 10000, 15000, 15000);
    Handle connection{WinHttpConnect(session.h, kHost, INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.h) return false;
    Handle req{WinHttpOpenRequest(connection.h, method, path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!req.h) return false;
    const auto size = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(req.h, headers.c_str(), static_cast<DWORD>(-1),
                            size ? const_cast<char*>(body.data()) : WINHTTP_NO_REQUEST_DATA, size, size, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr))
        return false;
    DWORD statusSize = sizeof(*status);
    if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             status, &statusSize, WINHTTP_NO_HEADER_INDEX))
        return false;
    reply->clear();
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(req.h, &available)) return false;
        if (available == 0) return true;
        if (reply->size() + available > (1u << 20)) return false;  // Sony's answers are tiny
        const size_t old = reply->size();
        reply->resize(old + available);
        DWORD read = 0;
        if (!WinHttpReadData(req.h, reply->data() + old, available, &read)) return false;
        reply->resize(old + read);
    }
}

void wipe(std::string& s) {  // tokens should not linger in freed memory
    SecureZeroMemory(s.data(), s.size());
    s.clear();
}

}  // namespace

extern const char kPsnLoginUrl[] =
    "https://auth.api.sonyentertainmentnetwork.com/2.0/oauth/authorize?service_entity=urn:service-entity:psn"
    "&response_type=code&client_id=ba495a24-818c-472b-b12d-ff231c1b5745"
    "&redirect_uri=https://remoteplay.dl.playstation.net/remoteplay/redirect"
    "&scope=psn:clientapp%20referenceDataService:countryConfig.read%20pushNotification:webSocket.desktop.connect"
    "%20sessionManager:remotePlaySession.system.update"
    "&request_locale=en_US&ui=pr&service_logo=ps&layout_type=popup&smcid=remoteplay&prompt=always"
    "&PlatformPrivacyWs1=minimal&";

std::optional<AccountId> fetchPsnAccountId(const std::string& code, std::string* errorRu) {
    const auto fail = [&](const char* message) -> std::optional<AccountId> {
        if (errorRu) *errorRu = message;
        return std::nullopt;
    };
    const std::string body = "grant_type=authorization_code&code=" + urlEncode(code) + "&scope=" + urlEncode(kScope) +
                             "&redirect_uri=" + urlEncode(kRedirect);
    const std::wstring postHeaders = std::wstring(kAuthHeader) + L"\r\nContent-Type: application/x-www-form-urlencoded";
    DWORD status = 0;
    std::string reply;
    if (!request(L"POST", kTokenPath, body, postHeaders, &status, &reply))
        return fail("Нет связи с сервером Sony. Проверьте интернет и попробуйте ещё раз.");
    if (status != 200) {
        wipe(reply);
        return fail("Sony не приняла код. Он одноразовый и действует пару минут: войдите ещё раз и сразу вставьте "
                    "новую ссылку.");
    }
    auto token = nlohmann::json::parse(reply, nullptr, false);
    wipe(reply);
    std::string access;
    if (token.is_object() && token.contains("access_token") && token["access_token"].is_string())
        access = token["access_token"].get<std::string>();
    token = nullptr;
    const bool safe = !access.empty() && access.size() < 512 &&  // goes into the URL path below
                      std::all_of(access.begin(), access.end(), [](char ch) {
                          return std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.';
                      });
    if (!safe) {
        wipe(access);
        return fail("Sony прислала непонятный ответ. Попробуйте ещё раз чуть позже.");
    }

    // The token's own info carries the account number ("user_id", decimal).
    const std::wstring path = std::wstring(kTokenPath) + L"/" + std::wstring(access.begin(), access.end());
    const bool answered = request(L"GET", path, {}, kAuthHeader, &status, &reply);
    wipe(access);
    if (!answered) return fail("Нет связи с сервером Sony. Проверьте интернет и попробуйте ещё раз.");
    const auto info = nlohmann::json::parse(reply, nullptr, false);
    wipe(reply);
    if (status != 200 || !info.is_object() || !info.contains("user_id") || !info["user_id"].is_string())
        return fail("Sony не отдала номер аккаунта. Попробуйте войти ещё раз.");
    const auto id = parseAccountId(info["user_id"].get<std::string>(), nullptr);
    if (!id) return fail("Sony прислала номер аккаунта в неожиданном виде.");
    return id;
}
