// SPDX-License-Identifier: AGPL-3.0-only
#include "win/sound.h"

#include <windows.h>
#include <mmsystem.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>

namespace {

constexpr int kRate = 44100;

struct Tone {
    double hz;  // 0 = silence
    int ms;
};

// Builds an in-memory 16-bit mono WAV of the given tones (5 ms fade in/out each).
std::vector<std::uint8_t> makeWav(std::initializer_list<Tone> tones) {
    std::vector<std::int16_t> samples;
    for (const Tone& t : tones) {
        const int n = kRate * t.ms / 1000;
        const int fade = kRate * 5 / 1000;
        for (int i = 0; i < n; ++i) {
            double env = 1.0;
            if (i < fade) env = static_cast<double>(i) / fade;
            if (n - i < fade) env = static_cast<double>(n - i) / fade;
            const double v = t.hz > 0 ? std::sin(2.0 * 3.14159265358979 * t.hz * i / kRate) : 0.0;
            samples.push_back(static_cast<std::int16_t>(v * env * 9000));
        }
    }
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(samples.size() * 2);
    std::vector<std::uint8_t> wav(44 + dataBytes);
    auto put32 = [&](size_t at, std::uint32_t v) { std::memcpy(&wav[at], &v, 4); };
    auto put16 = [&](size_t at, std::uint16_t v) { std::memcpy(&wav[at], &v, 2); };
    std::memcpy(&wav[0], "RIFF", 4);
    put32(4, 36 + dataBytes);
    std::memcpy(&wav[8], "WAVEfmt ", 8);
    put32(16, 16);
    put16(20, 1);  // PCM
    put16(22, 1);  // mono
    put32(24, kRate);
    put32(28, kRate * 2);
    put16(32, 2);
    put16(34, 16);
    std::memcpy(&wav[36], "data", 4);
    put32(40, dataBytes);
    std::memcpy(&wav[44], samples.data(), dataBytes);
    return wav;
}

const std::vector<std::uint8_t>& wavFor(Cue c) {
    // Built once; PlaySound(SND_MEMORY | SND_ASYNC) needs the buffer to outlive playback.
    static const std::vector<std::uint8_t> on = makeWav({{600, 70}, {900, 70}});
    static const std::vector<std::uint8_t> off = makeWav({{900, 70}, {600, 70}});
    static const std::vector<std::uint8_t> warn = makeWav({{300, 80}, {0, 50}, {300, 80}, {0, 50}, {300, 80}});
    static const std::vector<std::uint8_t> up = makeWav({{1000, 120}});
    static const std::vector<std::uint8_t> blip = makeWav({{750, 45}});
    switch (c) {
        case Cue::CaptureOn: return on;
        case Cue::CaptureOff: return off;
        case Cue::LinkUp: return up;
        case Cue::ClassChange: return blip;
        case Cue::FocusLost:
        case Cue::LinkLost: break;
    }
    return warn;
}

}  // namespace

void playCue(Cue c) {
    const auto& wav = wavFor(c);
    PlaySoundW(reinterpret_cast<LPCWSTR>(wav.data()), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}
