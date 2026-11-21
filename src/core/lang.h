// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <string_view>

// Interface language. Every user-facing text is written in both languages at the place it is used:
// tr("Подключено", "Connected"). Switching is instant; the app remembers the choice.
enum class Lang { Ru, En };

Lang uiLang();
void setUiLang(Lang lang);
std::string_view langCode(Lang lang);                 // "ru", "en"
Lang langFromCode(std::string_view code, Lang fallback);  // unknown -> fallback

inline const char* tr(const char* ru, const char* en) { return uiLang() == Lang::Ru ? ru : en; }
