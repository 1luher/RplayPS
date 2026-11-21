// SPDX-License-Identifier: AGPL-3.0-only
// PSN account ID through Sony's own sign-in page, the way chiaki-ng does it (psnaccountid.cpp):
// the user signs in in a browser, pastes the address of the "redirect" page back, and the one-time
// code in it is exchanged for the account number. Nothing is stored: the access token lives only for
// the two requests below.
#pragma once

#include <optional>
#include <string>

#include "core/account_id.h"

// Sony's sign-in page for the PS Remote Play app; open it in the default browser.
extern const char kPsnLoginUrl[];

// Blocking (two HTTPS requests to Sony, up to ~30 s). On failure returns nullopt and a Russian message.
std::optional<AccountId> fetchPsnAccountId(const std::string& code, std::string* errorRu);
