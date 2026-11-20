// SPDX-License-Identifier: AGPL-3.0-only
#include <doctest/doctest.h>

#include <windows.h>

#include "win/input_thread.h"
#include "win/sound.h"

TEST_CASE("capture_cues") {
    CHECK(cueForCapture(CaptureEvent::Captured) == Cue::CaptureOn);
    CHECK(cueForCapture(CaptureEvent::Released) == Cue::CaptureOff);
    CHECK(cueForCapture(CaptureEvent::FocusLost) == Cue::FocusLost);
    // The link-lost alarm is already playing; a capture-off sweep would cut it off.
    CHECK_FALSE(cueForCapture(CaptureEvent::LinkLost));
}

TEST_CASE("sticky_keys_hotkey_is_disabled_while_captured") {
    const DWORD original = SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY | SKF_AVAILABLE;
    const DWORD during = stickyKeysFlagsWhileCaptured(original);
    CHECK((during & SKF_HOTKEYACTIVE) == 0);
    CHECK((during & SKF_CONFIRMHOTKEY) == 0);
    CHECK((during & SKF_AVAILABLE) != 0);
}

TEST_CASE("sticky_keys_left_alone_when_user_turned_them_on") {
    const DWORD original = SKF_STICKYKEYSON | SKF_HOTKEYACTIVE;
    CHECK(stickyKeysFlagsWhileCaptured(original) == original);
}
