// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <string>

#include "core/lang.h"
#include "core/profile.h"

TEST_CASE("tr_follows_the_interface_language") {
    const Lang before = uiLang();
    setUiLang(Lang::En);
    CHECK(std::string(tr("Подключено", "Connected")) == "Connected");
    CHECK(controlClassTitle(ControlClass::Heli) == "Helicopter");
    setUiLang(Lang::Ru);
    CHECK(std::string(tr("Подключено", "Connected")) == "Подключено");
    CHECK(controlClassTitle(ControlClass::Heli) == "Вертолёт");
    setUiLang(before);
}

TEST_CASE("lang_code_roundtrip_and_fallback") {
    CHECK(langFromCode(langCode(Lang::Ru), Lang::En) == Lang::Ru);
    CHECK(langFromCode(langCode(Lang::En), Lang::Ru) == Lang::En);
    CHECK(langFromCode("en-US\r", Lang::Ru) == Lang::En);
    CHECK(langFromCode("", Lang::En) == Lang::En);
    CHECK(langFromCode("de", Lang::Ru) == Lang::Ru);
}
