// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

// Short audio cues: while playing, the monitor shows the PS5, so RplayPS's window is not visible.
enum class Cue { CaptureOn, CaptureOff, FocusLost, LinkUp, LinkLost, ClassChange };

// Asynchronous; never blocks the caller. A new cue cuts off the one still playing.
void playCue(Cue c);
