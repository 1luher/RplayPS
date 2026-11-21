// SPDX-License-Identifier: AGPL-3.0-only
#include "core/lang.h"

#include <atomic>

namespace {
std::atomic<Lang> g_lang{Lang::Ru};  // read from worker threads (link messages)
}

Lang uiLang() { return g_lang.load(std::memory_order_relaxed); }

void setUiLang(Lang lang) { g_lang.store(lang, std::memory_order_relaxed); }

std::string_view langCode(Lang lang) { return lang == Lang::Ru ? "ru" : "en"; }

Lang langFromCode(std::string_view code, Lang fallback) {
    if (code.substr(0, 2) == "ru") return Lang::Ru;
    if (code.substr(0, 2) == "en") return Lang::En;
    return fallback;
}
