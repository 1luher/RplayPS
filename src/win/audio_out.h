// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

// Plays the game's sound on the PC's default audio output (16-bit PCM, interleaved).
// Thread-safe: the format and the samples arrive on the Remote Play thread.
// Keeps latency bounded: about 30 ms of sound is buffered before playback starts, and frames are
// dropped once more than ~60 ms is queued (the network caught up after a stall).
class AudioOut {
public:
    AudioOut();
    ~AudioOut();
    AudioOut(const AudioOut&) = delete;
    AudioOut& operator=(const AudioOut&) = delete;

    void setFormat(unsigned channels, unsigned rate);  // (re)opens the output device
    void play(const std::int16_t* samples, std::size_t frames);
    void close();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
